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

static const char* XPC_VAL_STR = "val";

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
            if(-1 == clazz->mMemberCount) // -1 means 'failed to init'
            {
                NS_RELEASE(clazz);
                clazz = NULL;
            }
        }
    }
    return clazz;
}

nsXPCWrappedNativeClass::nsXPCWrappedNativeClass(XPCContext* xpcc, REFNSIID aIID,
                                               nsIInterfaceInfo* aInfo)
    : mXPCContext(xpcc),
      mIID(aIID),
      mInfo(aInfo),
      mMemberCount(-1),
      mMembers(NULL)
{
    NS_ADDREF(mInfo);

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    mXPCContext->GetWrappedNativeClassMap()->Add(this);
    if(!BuildMemberDescriptors())
        mMemberCount = -1;
}

nsXPCWrappedNativeClass::~nsXPCWrappedNativeClass()
{
    mXPCContext->GetWrappedNativeClassMap()->Remove(this);
    DestroyMemberDescriptors();
    NS_RELEASE(mInfo);
}

JSBool
nsXPCWrappedNativeClass::BuildMemberDescriptors()
{
    int i;
    int constCount;
    int methodCount;
    int totalCount;
    JSContext* cx = GetJSContext();

    if(NS_FAILED(mInfo->GetMethodCount(&methodCount))||
       NS_FAILED(mInfo->GetConstantCount(&constCount)))
        return JS_FALSE;

    totalCount = methodCount+constCount;
    if(!totalCount)
    {
        mMemberCount = 0;
        return JS_TRUE;
    }

    mMembers = new XPCNativeMemberDescriptor[totalCount];
    if(!mMembers)
        return JS_FALSE;
    mMemberCount = 0;

    for(i = 0; i < methodCount; i++)
    {
        jsval idval;
        jsid id;
        XPCNativeMemberDescriptor* desc;
        const nsXPCMethodInfo* info;
        if(NS_FAILED(mInfo->GetMethodInfo(i, &info)))
            return JS_FALSE;

        idval = STRING_TO_JSVAL(JS_InternString(cx, info->GetName()));
        JS_ValueToId(cx, idval, &id);
        if(!id)
        {
            NS_ASSERTION(0,"bad method name");
            return JS_FALSE;
        }

        if(info->IsSetter())
        {
            NS_ASSERTION(mMemberCount,"bad setter");
            desc = &mMembers[mMemberCount-1];
            NS_ASSERTION(desc->id == id,"bad setter");
            NS_ASSERTION(desc->category == XPCNativeMemberDescriptor::ATTRIB_RO,"bad setter");
            desc->category == XPCNativeMemberDescriptor::ATTRIB_RW;
            desc->index2 = i;
        }
        else
        {
            NS_ASSERTION(!LookupMemberByID(id),"duplicate method name");
            desc = &mMembers[mMemberCount++];
            desc->id = id;
            if(info->IsGetter())
                desc->category == XPCNativeMemberDescriptor::ATTRIB_RO;
            else
                desc->category == XPCNativeMemberDescriptor::METHOD;
            desc->index = i;
        }
    }

    // XXX do constants

    return JS_TRUE;
}

void
nsXPCWrappedNativeClass::DestroyMemberDescriptors()
{
    if(!mMembers)
        return;
    JSContext* cx = GetJSContext();
    for(int i = 0; i < mMemberCount; i++)
        if(mMembers[i].invokeFuncObj)
            JS_RemoveRoot(cx, &mMembers[i].invokeFuncObj);
    delete [] mMembers;
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

static JSBool
isConstructorID(JSContext *cx, jsid id)
{
    jsval idval;
    const char *property_name;

    if (JS_IdToValue(cx, id, &idval) && JSVAL_IS_STRING(idval) &&
        (property_name = JS_GetStringBytes(JSVAL_TO_STRING(idval))) != NULL) {
        if (!strcmp(property_name, "constructor")) {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

const char*
nsXPCWrappedNativeClass::GetMethodName(int MethodIndex) const
{
    const nsXPCMethodInfo* info;
    if(NS_SUCCEEDED(mInfo->GetMethodInfo(MethodIndex, &info)))
        return info->GetName();
    return NULL;
}

void
nsXPCWrappedNativeClass::SetDescriptorCounts(XPCNativeMemberDescriptor* desc)
{
    // XXX this is subject to serious improvement!
    switch(desc->category)
    {
    case XPCNativeMemberDescriptor::CONSTANT:
        NS_ASSERTION(0,"bad call");
        break;
    case XPCNativeMemberDescriptor::METHOD:
    {
        const nsXPCMethodInfo* info;
        if(NS_FAILED(mInfo->GetMethodInfo(desc->index, &info)))
            break;

        uintN scratchWords = 0;
        for(uint8 i = info->GetParamCount()-1 ; i >= 0; i--)
        {
            const nsXPCParamInfo& param = info->GetParam(i);
            if(param.IsOut())
            {
                // XXX what about space for IIDs?
                scratchWords += param.GetType().WordCount();
            }
        }
        desc->maxParamCount = info->GetParamCount();
        desc->maxScratchWordCount = scratchWords;
        break;
    }
    case XPCNativeMemberDescriptor::ATTRIB_RO:
    case XPCNativeMemberDescriptor::ATTRIB_RW:
        // just set these for the max possible...
        desc->maxParamCount = 1;
        desc->maxScratchWordCount = 2;
        break;
    default:
        NS_ASSERTION(0,"bad category");
        break;
    }
}

JSBool
nsXPCWrappedNativeClass::GetConstantAsJSVal(nsXPCWrappedNative* wrapper,
                                            const XPCNativeMemberDescriptor* desc,
                                            jsval* vp)
{
    // XXX implement
    return JS_FALSE;

}

void
nsXPCWrappedNativeClass::ReportError(const XPCNativeMemberDescriptor* desc,
                                     const char* msg)
{
    // XXX implement
}


JSBool
nsXPCWrappedNativeClass::CallWrappedMethod(nsXPCWrappedNative* wrapper,
                                           const XPCNativeMemberDescriptor* desc,
                                           JSBool isAttributeSet,
                                           uintN argc, jsval *argv, jsval *vp)
{
#define PARAM_COUNT     32
#define SCRATCH_WORDS   32

    nsXPCVarient paramBuffer[PARAM_COUNT];
    uint32 scratchBuffer[SCRATCH_WORDS];
    JSBool retval = JS_FALSE;

    nsXPCVarient* dispatchParams = NULL;
    uint32* scratch = NULL;
    uint32 scratchIndex = 0;
    JSContext* cx = GetJSContext();
    uint8 i;
    const nsXPCMethodInfo* info;
    uint8 requiredArgs;
    uint8 paramCount = info->GetParamCount();
    jsval src;
    jsdouble num;
    uint8 vtblIndex;
    nsresult invokeResult;

    // setup buffers

    if(GetMaxParamCount(desc) > PARAM_COUNT)
        dispatchParams = new nsXPCVarient[GetMaxParamCount(desc)];
    else
        dispatchParams = paramBuffer;

    if(GetMaxScratchWordCount(desc) > SCRATCH_WORDS)
        scratch = new uint32[GetMaxScratchWordCount(desc)];
    else
        scratch = scratchBuffer;
    if(!dispatchParams || !scratch)
    {
        ReportError(desc, "out of memeory");
        goto done;
    }

    // make sure we have what we need

    if(isAttributeSet)
    {
        // fail silently if trying to write a readonly attribute
        if(desc->category != XPCNativeMemberDescriptor::ATTRIB_RW)
            goto done;
        vtblIndex = desc->index2;
    }
    else
        vtblIndex = desc->index;

    if(NS_FAILED(mInfo->GetMethodInfo(vtblIndex, &info)))
    {
        ReportError(desc, "can't get MethodInfo");
        goto done;
    }

    // XXX ASSUMES that retval is last arg.
    requiredArgs = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval()) ? 0 : 1;
    if(argc < requiredArgs)
    {
        ReportError(desc, "not enough arguments");
        goto done;
    }

    // iterate through the params doing conversions
    for(i = 0; i < paramCount; i++)
    {
        const nsXPCParamInfo& param = info->GetParam(i);
        const nsXPCType& type = param.GetType();

        nsXPCVarient* dp;

        dispatchParams[i].type = type;

        if(param.IsOut())
        {
            dispatchParams[i].val.p = &scratch[scratchIndex];
            scratchIndex += type.WordCount();
            if(!param.IsIn())
                continue;
            dp = (nsXPCVarient*)dispatchParams[i].val.p;
        }
        else
            dp = &dispatchParams[i];

        // XXX fix this mess...

        if(type & ~nsXPCType::TYPE_MASK)
        {
            NS_ASSERTION(0,"not supported");
            continue;
        }

        // set 'src' to be the object from which we get the value

        if(param.IsOut() /* implicit && param.IsIn() */ )
            src = argv[i];
        else if(!JSVAL_IS_OBJECT(argv[i]) ||
                !JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &src))
        {
            ReportError(desc, "no out val");
            goto done;
        }

        // XXX just ASSUME a number

        if(!JS_ValueToNumber(cx, src, &num))
        {
            ReportError(desc, "could not convert argument to a number");
            goto done;
        }

        switch(type)
        {
        case nsXPCType::T_I8     : dp->val.i8  = (int8)    num;  break;
        case nsXPCType::T_I16    : dp->val.i16 = (int16)   num;  break;
        case nsXPCType::T_I32    : dp->val.i32 = (int32)   num;  break;
        case nsXPCType::T_I64    : dp->val.i64 = (int64)   num;  break;
        case nsXPCType::T_U8     : dp->val.u8  = (uint8)   num;  break;
        case nsXPCType::T_U16    : dp->val.u16 = (uint16)  num;  break;
        case nsXPCType::T_U32    : dp->val.u32 = (uint32)  num;  break;
        case nsXPCType::T_U64    : dp->val.u64 = (uint64)  num;  break;
        case nsXPCType::T_FLOAT  : dp->val.f   = (float)   num;  break;
        case nsXPCType::T_DOUBLE : dp->val.d   = (double)  num;  break;
        case nsXPCType::T_BOOL   : dp->val.b   = (PRBool)  num;  break;
        case nsXPCType::T_CHAR   : dp->val.c   = (char)    num;  break;
        case nsXPCType::T_WCHAR  : dp->val.wc  = (wchar_t) num;  break;
        default:
            NS_ASSERTION(0, "bad type");
            ReportError(desc, "could not convert argument to a number");
            goto done;
        }
    }

    // do the invoke
    invokeResult = xpc_InvokeNativeMethod(wrapper->GetNative(),vtblIndex,
                                          paramCount, dispatchParams);


    // iterate through the params to gather the results

    // set the retval for success


done:
    if(dispatchParams && dispatchParams != paramBuffer)
        delete [] dispatchParams;
    if(scratch && scratch != scratchBuffer)
        delete [] scratch;
    return retval;
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
            NS_CONST_CAST(XPCNativeMemberDescriptor*,desc);

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
    {
        if(isConstructorID(cx, id))
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
//                                   GetXPCContext()->GetGlobalObject());
    if(!jsobj || !JS_SetPrivate(cx, jsobj, self))
        return NULL;
    return jsobj;
}

