/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Class shared by all native interface instances. */

#include "xpcprivate.h"

NS_IMPL_ISUPPORTS(nsXPCWrappedNativeClass, NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID)

// static 
nsXPCWrappedNativeClass* 
nsXPCWrappedNativeClass::GetNewOrUsedClass(XPCContext* xpcc,
                                          REFNSIID aIID)
{
    IID2WrappedNativeClassMap* map;
    nsXPCWrappedNativeClass* clazz;
    
    NS_PRECONDITION(xpcc, "bad param");
    
    map = xpcc->GetWrappedNativeClassMap();
    NS_ASSERTION(map,"bad map");

    clazz = map->Find(aIID);
    if(clazz)
    {
        NS_ADDREF(clazz);
    }
    else
    {
        nsIInterfaceInfo* info;
        nsXPConnect* xpc;
        if((xpc = nsXPConnect::GetXPConnect()) != NULL &&
           NS_SUCCEEDED(xpc->GetInterfaceInfo(aIID, &info)))
        {
            clazz = new nsXPCWrappedNativeClass(xpcc, aIID, info);
            // XXX check for full init
            // if failed: NS_RELEASE(map) and set map = NULL
        }
    }
    return clazz;    
}

nsXPCWrappedNativeClass::nsXPCWrappedNativeClass(XPCContext* xpcc, REFNSIID aIID, 
                                               nsIInterfaceInfo* aInfo)
    : mXPCContext(xpcc),
      mIID(aIID),
      mInfo(aInfo)
{
    NS_ADDREF(mInfo);

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mXPCContext->GetWrappedNativeClassMap()->Add(this);

    // XXX Do other stuff...
}            

nsXPCWrappedNativeClass::~nsXPCWrappedNativeClass()
{
    mXPCContext->GetWrappedNativeClassMap()->Remove(this);

    // XXX Cleanup other stuff...
    // XXX e.g. mMembers
    // XXX e.g. functon object of mbers
    NS_RELEASE(mInfo);
}            

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    nsXPCWrappedNative* wrapper;
    
    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if (!wrapper) {
        if (type == JSTYPE_OBJECT) {
            *vp = OBJECT_TO_JSVAL(obj);
            return JS_TRUE;
        }
        
        JS_ReportError(cx, "illegal operation on WrappedNative prototype object");
        return JS_FALSE;
    }

    switch (type) {
    case JSTYPE_OBJECT:
        *vp = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;

    case JSTYPE_FUNCTION:
        JS_ReportError(cx, "can't convert WrappedNative to function");
        return JS_FALSE;

    case JSTYPE_VOID:
    case JSTYPE_STRING:
        // XXX get the interface name from the InterfaceInfo
        *vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "WrappedNative"));
        return JS_TRUE;

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

const XPCNativeMemberDescriptor*
nsXPCWrappedNativeClass::LookupMemberByID(jsid id) const
{
    for(int i = GetMemberCount()-1; i >= 0; i--)
    {
        const XPCNativeMemberDescriptor* desc = GetMemberDescriptor(i);
        NS_ASSERTION(desc,"member without descriptor");
        if(desc->id == id)
            return desc;
    }
    return NULL;
}        

const char* 
nsXPCWrappedNativeClass::GetMethodName(int MethodIndex) const
{
    const nsXPCMethodInfo* info;
    if(NS_SUCCEEDED(mInfo->GetMethodInfo(MethodIndex, &info)))
        return info->GetName();
    return NULL;    
}        

JSBool 
nsXPCWrappedNativeClass::GetConstantAsJSVal(nsXPCWrappedNative* wrapper, 
                                            const XPCNativeMemberDescriptor* desc, 
                                            jsval* vp)
{
    // XXX implement
    return JS_FALSE;
}

JSBool 
nsXPCWrappedNativeClass::GetAttributeAsJSVal(nsXPCWrappedNative* wrapper, 
                                             const XPCNativeMemberDescriptor* desc, 
                                             jsval* vp)
{
    return CallWrappedMethod(wrapper, desc, JS_FALSE, 0, NULL, vp);
}

JSBool 
nsXPCWrappedNativeClass::SetAttributeFromJSVal(nsXPCWrappedNative* wrapper, 
                                               const XPCNativeMemberDescriptor* desc, 
                                               jsval* vp)
{
    return CallWrappedMethod(wrapper, desc, JS_TRUE, 1, vp, NULL);
}

JSBool 
nsXPCWrappedNativeClass::CallWrappedMethod(nsXPCWrappedNative* wrapper, 
                                           const XPCNativeMemberDescriptor* desc, 
                                           JSBool isAttributeSet,
                                           uintN argc, jsval *argv, jsval *vp)
{
    // XXX implement
    return JS_FALSE;
}


JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_CallMethod(JSContext *cx, JSObject *obj,
                         uintN argc, jsval *argv, jsval *vp)
{
    JSFunction *fun;
    jsint id;
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
    if(!desc || desc->category != XPCNativeMemberDescriptor::METHOD)
        return JS_FALSE;

    return clazz->CallWrappedMethod(wrapper, desc, JS_FALSE, argc, argv, vp);
}

JSObject* 
nsXPCWrappedNativeClass::GetInvokeFunObj(const XPCNativeMemberDescriptor* desc)
{
    if(!desc->invokeFuncObj)
    {
        const char* name = GetMethodName(desc->index);
        NS_ASSERTION(name,"bad method name");

        JSContext* cx = GetJSContext();

        JSFunction *fun = JS_NewFunction(cx, WrappedNative_CallMethod, 0,
                                         JSFUN_BOUND_METHOD, NULL, name);
        if(!fun)
            return NULL;

        XPCNativeMemberDescriptor* descRW = 
            NS_CONST_CAST(XPCNativeMemberDescriptor*,descRW);

        descRW->invokeFuncObj = JS_GetFunctionObject(fun);
        // XXX verify released in dtor
        JS_AddRoot(cx, &descRW->invokeFuncObj);
    }
    return desc->invokeFuncObj;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_getPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    nsXPCWrappedNative* wrapper;
    
    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(!wrapper)
        return JS_FALSE;

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    const XPCNativeMemberDescriptor* desc = clazz->LookupMemberByID(id);
    if(!desc)
        return JS_FALSE;
    
    switch(desc->category)
    {
    case XPCNativeMemberDescriptor::CONSTANT:
        return clazz->GetConstantAsJSVal(wrapper, desc, vp);

    case XPCNativeMemberDescriptor::METHOD:
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

    case XPCNativeMemberDescriptor::ATTRIB_RO:
    case XPCNativeMemberDescriptor::ATTRIB_RW:
        return clazz->GetAttributeAsJSVal(wrapper, desc, vp);

    default:        
        NS_ASSERTION(0,"bad category");
        return JS_FALSE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_setPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    nsXPCWrappedNative* wrapper;
    
    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if(!wrapper)
        return JS_FALSE;

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    const XPCNativeMemberDescriptor* desc = clazz->LookupMemberByID(id);
    if(!desc)
        return JS_FALSE;
    
    switch(desc->category)
    {
    case XPCNativeMemberDescriptor::CONSTANT:
    case XPCNativeMemberDescriptor::METHOD:
    case XPCNativeMemberDescriptor::ATTRIB_RO:
        // fail silently
        return JS_TRUE;

    case XPCNativeMemberDescriptor::ATTRIB_RW:
        return clazz->SetAttributeFromJSVal(wrapper, desc, vp);
    default:        
        NS_ASSERTION(0,"bad category");
        return JS_FALSE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_lookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
                            )
{
    nsXPCWrappedNative* wrapper;
    
    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
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
    }

    *objp = NULL;
    *propp = NULL;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_defineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                         JSPropertyOp getter, JSPropertyOp setter,
                         uintN attrs, JSProperty **propp)
{
    // XXX dynamic properties not supported
    JS_ReportError(cx, "Cannot define a new property in a WrappedNative");
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_getAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    // XXX dynamic properties not supported
    /* We don't maintain JS property attributes for Java class members */
    *attrsp = JSPROP_PERMANENT|JSPROP_ENUMERATE;
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_setAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    // XXX dynamic properties not supported
    /* Silently ignore all setAttribute attempts */
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_deleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    // XXX dynamic properties not supported
    /* Silently ignore */
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_defaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    return WrappedNative_convert(cx, obj, type, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_newEnumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                        jsval *statep, jsid *idp)
{
    nsXPCWrappedNative* wrapper;
    
    wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx, obj);
    if (!wrapper) {
        *statep = JSVAL_NULL;
        if (idp)
            *idp = INT_TO_JSVAL(0);
        return JS_TRUE;
    }

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    switch(enum_op) {
    case JSENUMERATE_INIT:
        *statep = INT_TO_JSVAL(0);
        if (idp)
            *idp = INT_TO_JSVAL(clazz->GetMemberCount());
        return JS_TRUE;
        
    case JSENUMERATE_NEXT:
    {
        int index = JSVAL_TO_INT(*statep);
        int count = clazz->GetMemberCount();

        if (index < count) {
            *idp = INT_TO_JSVAL(clazz->GetMemberDescriptor(index++)->id);
            *statep = INT_TO_JSVAL(index < count ? index : 0);
            return JS_TRUE;
        }
    }

    /* Fall through ... */

    case JSENUMERATE_DESTROY:
        *statep = JSVAL_NULL;
        return JS_TRUE;

    default:
        NS_ASSERTION(0,"bad enum_op");
        return JS_FALSE;
    }
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_checkAccess(JSContext *cx, JSObject *obj, jsid id,
                      JSAccessMode mode, jsval *vp, uintN *attrsp)
{
    switch (mode) {
    case JSACC_WATCH:
        JS_ReportError(cx, "Cannot place watchpoints on WrappedNative object properties");
        return JS_FALSE;

    case JSACC_IMPORT:
        JS_ReportError(cx, "Cannot export a WrappedNative object's properties");
        return JS_FALSE;

    default:
        return JS_TRUE;
    }
}


JS_STATIC_DLL_CALLBACK(void)
WrappedNative_finalize(JSContext *cx, JSObject *obj)
{
    nsXPCWrappedNative* wrapper = (nsXPCWrappedNative*) JS_GetPrivate(cx,obj);
    if(!wrapper)
        return;
    NS_ASSERTION(obj == wrapper->GetJSObject(),"bad obj");
    NS_ASSERTION(cx == wrapper->GetClass()->GetXPCContext()->GetJSContext(),"bad obj");
    wrapper->JSObjectFinalized();
}

JSObjectOps WrappedNative_ops = {
    /* Mandatory non-null function pointer members. */
    NULL,                       /* newObjectMap */
    NULL,                       /* destroyObjectMap */
    WrappedNative_lookupProperty,
    WrappedNative_defineProperty,
    WrappedNative_getPropertyById, /* getProperty */
    WrappedNative_setPropertyById, /* setProperty */
    WrappedNative_getAttributes,
    WrappedNative_setAttributes,
    WrappedNative_deleteProperty,
    WrappedNative_defaultValue,
    WrappedNative_newEnumerate,
    WrappedNative_checkAccess,

    /* Optionally non-null members start here. */
    NULL,                       /* thisObject */
    NULL,                       /* dropProperty */
    NULL,                       /* call */
    NULL,                       /* construct */
    NULL,                       /* xdrObject */
    NULL,                       /* hasInstance */
};

static JSObjectOps *
WrappedNative_getObjectOps(JSContext *cx, JSClass *clazz)
{
    return &WrappedNative_ops;
}

JSClass WrappedNative_class = {
    "XPCWrappedNative", JSCLASS_HAS_PRIVATE,
    NULL, NULL, NULL, NULL,
    NULL, NULL, WrappedNative_convert, WrappedNative_finalize,
    WrappedNative_getObjectOps,
};

extern "C" JS_IMPORT_DATA(JSObjectOps) js_ObjectOps;

// static 
JSBool 
nsXPCWrappedNativeClass::InitForContext(XPCContext* xpcc)
{
    WrappedNative_ops.newObjectMap = js_ObjectOps.newObjectMap;
    WrappedNative_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;

    // XXX do we really want this class init'd this way? access to ctor?

    if (!JS_InitClass(xpcc->GetJSContext(), xpcc->GetGlobalObject(), 
        0, &WrappedNative_class, 0, 0,
        0, 0,
        0, 0))
        return JS_FALSE;
    return JS_TRUE;
}

JSObject* 
nsXPCWrappedNativeClass::NewInstanceJSObject(nsXPCWrappedNative* self)
{
    JSContext* cx = GetXPCContext()->GetJSContext();
    JSObject* jsobj = JS_NewObject(cx, &WrappedNative_class, NULL, NULL);
    if(!jsobj || !JS_SetPrivate(cx, jsobj, self))
        return NULL;
    return jsobj;
}        

