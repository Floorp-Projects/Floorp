/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* JavaScript Object Ops for our Wrapped Native JS Objects. */

#include "xpcprivate.h"

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

static void ThrowException(uintN errNum, JSContext* cx)
    {nsXPConnect::GetJSThrower()->ThrowException(errNum, cx);}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);

    if (!wrapper) {
        if (type == JSTYPE_OBJECT) {
            *vp = OBJECT_TO_JSVAL(obj);
            return JS_TRUE;
        }
        ThrowException(XPCJSError::BAD_OP_ON_WN_PROTO, cx);
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
        ThrowException(XPCJSError::CANT_CONVERT_WN_TO_FUN, cx);
        return JS_FALSE;

    case JSTYPE_VOID:
    case JSTYPE_STRING:
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        
        XPCContext* xpcc;
        const XPCNativeMemberDescriptor* desc;

        if(nsnull != (xpcc = clazz->GetXPCContext()) &&
           nsnull != (desc = clazz->LookupMemberByID(
                           xpcc->GetStringID(XPCContext::IDX_TO_STRING))) &&
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
    JSFunction *fun;
    jsid id;
    jsval idval;

    nsXPCWrappedNative* wrapper;
    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
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
        return JS_FALSE;

    return clazz->CallWrappedMethod(cx, wrapper, desc, 
                                    nsXPCWrappedNativeClass::CALL_METHOD, 
                                    argc, argv, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    nsXPCWrappedNative* wrapper;

    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(!wrapper)
    {
        XPCContext* xpcc = nsXPConnect::GetContext(cx);
        if(xpcc && id == xpcc->GetStringID(XPCContext::IDX_CONSTRUCTOR))
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
            return clazz->GetConstantAsJSVal(cx, wrapper, desc, vp);
        else if(desc->IsMethod())
        {
            // allow for lazy creation of 'prototypical' function invoke object
            JSObject* invokeFunObj = clazz->GetInvokeFunObj(desc);
            if(!invokeFunObj)
                return JS_FALSE;
            // Create a function object with this wrapper as its parent, so that
            // JSFUN_BOUND_METHOD binds it as the default 'this' for the function.
            JSObject *funobj = JS_CloneFunctionObject(cx, invokeFunObj, obj);
            if (!funobj)
                return JS_FALSE;
            *vp = OBJECT_TO_JSVAL(funobj);
            return JS_TRUE;
        }
        else    // attribute
            return clazz->GetAttributeAsJSVal(cx, wrapper, desc, vp);
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
        // XXX silently fail when property not found or call fails?
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    nsXPCWrappedNative* wrapper;

    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(!wrapper)
        return JS_FALSE;

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    const XPCNativeMemberDescriptor* desc = clazz->LookupMemberByID(id);
    if(desc)
    {
        if(desc->IsWritableAttribute())
            return clazz->SetAttributeFromJSVal(cx, wrapper, desc, vp);
        else
            return JS_TRUE; // fail silently
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
        }
        // fail silently
        return JS_TRUE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_LookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
                            )
{
    AUTO_PUSH_JSCONTEXT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(wrapper)
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        if(clazz->LookupMemberByID(id))
        {
            *objp = obj;
            *propp = (JSProperty*)1;
            return JS_TRUE;
        }
        else if(nsnull != (ds = wrapper->GetDynamicScriptable()) &&
                nsnull != (as = wrapper->GetArbitraryScriptable()))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->LookupProperty(cx, obj, id, objp, propp,
                                               wrapper, as, &retval)))
                return retval;
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
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(wrapper && 
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
    ThrowException(XPCJSError::CANT_DEFINE_PROP_ON_WN, cx);
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_GetAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(wrapper && 
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
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(wrapper && 
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
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(wrapper && 
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
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_DefaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(wrapper && 
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
    nsXPCWrappedNative* wrapper;
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;

    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if (!wrapper) {
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
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(wrapper && 
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
        ThrowException(XPCJSError::CANT_WATCH_WN_STATIC, cx);
        return JS_FALSE;

    case JSACC_IMPORT:
        ThrowException(XPCJSError::CANT_EXPORT_WN_STATIC, cx);
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
    // this is a hack to get the obj of the actual object not the object
    // that JS thinks is the 'this' (which it passes as 'obj').
    if(!(obj = (JSObject*)argv[-2]))
        return JS_FALSE;

    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx,obj);
    if(wrapper && 
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        JSBool retval;
        if(NS_SUCCEEDED(ds->Call(cx, obj, argc, argv, rval,
                        wrapper, as, &retval)))
            return retval;
        ThrowException(XPCJSError::SCRIPTABLE_CALL_FAILED, cx);
    }
    else
        ThrowException(XPCJSError::CANT_CALL_WO_SCRIPTABLE, cx);
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_Construct(JSContext *cx, JSObject *obj,
                        uintN argc, jsval *argv, jsval *rval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    // this is a hack to get the obj of the actual object not the object
    // that JS thinks is the 'this' (which it passes as 'obj').
    if(!(obj = (JSObject*)argv[-2]))
        return JS_FALSE;

    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx,obj);
    if(wrapper && 
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        JSBool retval;
        if(NS_SUCCEEDED(ds->Construct(cx, obj, argc, argv, rval,
                                      wrapper, as, &retval)))
            return retval;
        ThrowException(XPCJSError::SCRIPTABLE_CTOR_FAILED, cx);
    }
    else
        ThrowException(XPCJSError::CANT_CTOR_WO_SCRIPTABLE, cx);
    return JS_FALSE;
}

// this is the final resting place of non-handled hasInstance calls
JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_ClassHasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    //XXX our default policy is to just say no. Is this right?
    *bp = JS_FALSE;
    return JS_TRUE;
}        

// this is in the ObjectOps and is called first
JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx,obj);
    if(wrapper && 
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
    AUTO_PUSH_JSCONTEXT(cx);
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx,obj);
    if(!wrapper)
        return;
    NS_ASSERTION(obj == wrapper->GetJSObject(),"bad obj");
    // wrapper is responsible for calling DynamicScriptable->Finalize
    wrapper->JSObjectFinalized(cx, obj);
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
    nsnull,                     /* dropProperty */
    nsnull,                     /* call         */
    nsnull,                     /* construct    */
    nsnull,                     /* xdrObject    */
    WrappedNative_HasInstance   /* hasInstance  */
};

static JSObjectOps WrappedNativeWithCall_ops = {
    /* Mandatory non-null function pointer members. */
    nsnull,                       /* filled in at runtime! - newObjectMap */
    nsnull,                       /* filled in at runtime! - destroyObjectMap */
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
    nsnull,                     /* dropProperty */
    WrappedNative_Call,         /* call         */
    WrappedNative_Construct,    /* construct    */
    nsnull,                     /* xdrObject    */
    WrappedNative_HasInstance   /* hasInstance  */
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
    "XPCWrappedNative", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
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
    "XPCWrappedNativeWithCall", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
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


JSBool xpc_WrappedNativeJSOpsOneTimeInit()
{
    WrappedNative_ops.newObjectMap     = js_ObjectOps.newObjectMap;
    WrappedNative_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;
    WrappedNative_ops.dropProperty     = js_ObjectOps.dropProperty;

    WrappedNativeWithCall_ops.newObjectMap     = js_ObjectOps.newObjectMap;
    WrappedNativeWithCall_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;
    WrappedNativeWithCall_ops.dropProperty     = js_ObjectOps.dropProperty;

    return JS_TRUE;
}    
