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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Class shared by all native interface instances. */

#include "xpcprivate.h"

const char* XPC_VAL_STR = "val";

NS_IMPL_ISUPPORTS(nsXPCWrappedNativeClass, NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID)

// static
nsXPCWrappedNativeClass*
nsXPCWrappedNativeClass::GetNewOrUsedClass(XPCContext* xpcc,
                                          REFNSIID aIID)
{
    IID2WrappedNativeClassMap* map;
    nsXPCWrappedNativeClass* clazz = NULL;

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
            desc->category = XPCNativeMemberDescriptor::ATTRIB_RW;
            desc->index2 = i;
        }
        else
        {
            NS_ASSERTION(!LookupMemberByID(id),"duplicate method name");
            desc = &mMembers[mMemberCount++];
            desc->id = id;
            if(info->IsGetter())
                desc->category = XPCNativeMemberDescriptor::ATTRIB_RO;
            else
                desc->category = XPCNativeMemberDescriptor::METHOD;
            desc->index = i;
        }
    }

    for(i = 0; i < constCount; i++)
    {
        jsval idval;
        jsid id;
        XPCNativeMemberDescriptor* desc;
        const nsXPCConstant* constant;
        if(NS_FAILED(mInfo->GetConstant(i, &constant)))
            return JS_FALSE;

        idval = STRING_TO_JSVAL(JS_InternString(cx, constant->GetName()));
        JS_ValueToId(cx, idval, &id);
        if(!id)
        {
            NS_ASSERTION(0,"bad constant name");
            return JS_FALSE;
        }

        NS_ASSERTION(!LookupMemberByID(id),"duplicate method/constant name");
        desc = &mMembers[mMemberCount++];
        desc->id = id;
        desc->category = XPCNativeMemberDescriptor::CONSTANT;
        desc->index = i;
    }

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
        // XXX perhaps more expressive toString?
        *vp = STRING_TO_JSVAL(
                JS_NewStringCopyZ(cx, wrapper->GetClass()->GetInterfaceName()));
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
nsXPCWrappedNativeClass::GetMemberName(const XPCNativeMemberDescriptor* desc) const
{
    switch(desc->category)
    {
    case XPCNativeMemberDescriptor::CONSTANT:
    {
        const nsXPCConstant* constant;
        if(NS_SUCCEEDED(mInfo->GetConstant(desc->index, &constant)))
            return constant->GetName();
        break;
    }
    case XPCNativeMemberDescriptor::METHOD:
    case XPCNativeMemberDescriptor::ATTRIB_RO:
    case XPCNativeMemberDescriptor::ATTRIB_RW:
    {
        const nsXPCMethodInfo* info;
        if(NS_SUCCEEDED(mInfo->GetMethodInfo(desc->index, &info)))
            return info->GetName();
        break;
    }
    default:
        NS_ASSERTION(0,"bad category");
        break;
    }

    return NULL;
}

const char*
nsXPCWrappedNativeClass::GetInterfaceName() const
{
    const char* name;
    mInfo->GetName(&name);
    return name;
}

#define JAM_DOUBLE(v,d) (d = (jsdouble)v, DOUBLE_TO_JSVAL(&d))
#define FIT_32(i,d) (INT_FITS_IN_JSVAL(i) ? INT_TO_JSVAL(i) : JAM_DOUBLE(i,d))
// Win32 can't handle uint64 to double conversion
#define JAM_DOUBLE_U64(v,d) JAM_DOUBLE(((int64)v),d)

JSBool
nsXPCWrappedNativeClass::GetConstantAsJSVal(nsXPCWrappedNative* wrapper,
                                            const XPCNativeMemberDescriptor* desc,
                                            jsval* vp)
{
    const nsXPCConstant* constant;

    NS_ASSERTION(desc->category == XPCNativeMemberDescriptor::CONSTANT,"bad type");
    if(NS_FAILED(mInfo->GetConstant(desc->index, &constant)))
    {
        // XX fail silently?
        *vp = JSVAL_NULL;
        return JS_TRUE;
    }
    jsdouble d;
    const nsXPCVarient& var = constant->GetValue();

    switch(var.type)
    {
    case nsXPCType::T_I8     : *vp = INT_TO_JSVAL((int32)var.val.i8);   break;
    case nsXPCType::T_I16    : *vp = INT_TO_JSVAL((int32)var.val.i16);  break;
    case nsXPCType::T_I32    : *vp = FIT_32(var.val.i32,d);             break;
    case nsXPCType::T_I64    : *vp = JAM_DOUBLE(var.val.i64,d);         break;
    case nsXPCType::T_U8     : *vp = INT_TO_JSVAL((int32)var.val.u8);   break;
    case nsXPCType::T_U16    : *vp = INT_TO_JSVAL((int32)var.val.u16);  break;
    case nsXPCType::T_U32    : *vp = FIT_32(var.val.u32,d);             break;
    case nsXPCType::T_U64    : *vp = JAM_DOUBLE_U64(var.val.u64,d);     break;
    case nsXPCType::T_FLOAT  : *vp = JAM_DOUBLE(var.val.f,d);           break;
    case nsXPCType::T_DOUBLE : *vp = DOUBLE_TO_JSVAL(&var.val.d);       break;
    case nsXPCType::T_BOOL   : *vp = var.val.b?JSVAL_TRUE:JSVAL_FALSE;  break;
    case nsXPCType::T_CHAR   : *vp = INT_TO_JSVAL((int32)var.val.c);    break;
    case nsXPCType::T_WCHAR  : *vp = INT_TO_JSVAL((int32)var.val.wc);   break;
    default:
        // XXX need to support string constants
        NS_ASSERTION(0, "bad type");
        ReportError(desc, "invalid constant type");
    }
    return JS_TRUE;
}

void
nsXPCWrappedNativeClass::ReportError(const XPCNativeMemberDescriptor* desc,
                                     const char* msg)
{
    JS_ReportError(GetJSContext(), "'%s' accessing '%s' of '%s'",
                   msg, GetMemberName(desc), GetInterfaceName());
}


JSBool
nsXPCWrappedNativeClass::CallWrappedMethod(nsXPCWrappedNative* wrapper,
                                           const XPCNativeMemberDescriptor* desc,
                                           JSBool isAttributeSet,
                                           uintN argc, jsval *argv, jsval *vp)
{
#define PARAM_BUFFER_COUNT     32

    nsXPCVarient paramBuffer[PARAM_BUFFER_COUNT];
    JSBool retval = JS_FALSE;

    nsXPCVarient* dispatchParams = NULL;
    JSContext* cx = GetJSContext();
    uint8 i;
    const nsXPCMethodInfo* info;
    uint8 requiredArgs;
    uint8 paramCount;
    uint8 dispatchParamsInitedCount = 0;
    jsval src;
    uint8 vtblIndex;
    nsresult invokeResult;

    *vp = JSVAL_NULL;
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
    paramCount = info->GetParamCount();
    requiredArgs = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);
    if(argc < requiredArgs)
    {
        ReportError(desc, "not enough arguments");
        goto done;
    }

    // setup varient array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
    {
        if(!(dispatchParams = new nsXPCVarient[paramCount]))
        {
            ReportError(desc, "out of memory");
            goto done;
        }
    }
    else
        dispatchParams = paramBuffer;

    // iterate through the params doing conversions
    for(i = 0; i < paramCount; i++)
    {
        const nsXPCParamInfo& param = info->GetParam(i);
        const nsXPCType& type = param.GetType();

        nsXPCVarient* dp = &dispatchParams[i];
        dp->type = type;
        dp->flags = 0;
        dp->val.p = NULL;
        dispatchParamsInitedCount++;

        // set 'src' to be the object from which we get the value and
        // prepare for out param

        if(param.IsOut())
        {
            dp->flags = nsXPCVarient::PTR_IS_DATA;

            // XXX are there no type alignment issues here?
            if(type & nsXPCType::IS_POINTER)
            {
                dp->ptr = &dp->ptr2;
                dp->ptr2 = &dp->val;
            }
            else
                dp->ptr = &dp->val;

            if(!param.IsRetval() &&
               (!JSVAL_IS_OBJECT(argv[i]) ||
                !JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &src)))
            {
                ReportError(desc, "out argument must be object");
                goto done;
            }
            if(!param.IsIn())
            {
                // We don't need to convert the jsval.

                // XXX for an out param *I think* I might need to set the flags
                // to VAL_IS_OWNED (after clearing val.p) if the type is
                // 'IS_POINTER' because I think the caller may have to take
                // over ownership of a pointer. But, this depends on the
                // type of the thing.

                continue;
            }
        }
        else
        {
            if(type & nsXPCType::IS_POINTER)
            {
                dp->ptr = &dp->val;
                dp->flags = nsXPCVarient::PTR_IS_DATA;
            }
            src = argv[i];
        }

        // do the actual conversion...

        // handle special cases first

        if(type == nsXPCType::T_INTERFACE)
        {
            // XXX implement INTERFACE

            // make sure 'src' is an object
            // get the nsIInterfaceInfo* from the param and
            // build a wrapper and then hand over the wrapper.
            // XXX remember to release the wrapper in cleanup below

            NS_ASSERTION(0,"interface params not supported");
            continue;
        }
        else if(type == nsXPCType::T_INTERFACE_IS)
        {
            // XXX implement INTERFACE_IS
            NS_ASSERTION(0,"interface_is params not supported");
            continue;
        }
        else if(type == nsXPCType::T_STRING)
        {
            // XXX implement STRING
            NS_ASSERTION(0,"string params not supported");
            continue;
        }
        else if(type == nsXPCType::T_P_IID)
        {
            // XXX implement IID
            NS_ASSERTION(0,"iid params not supported");
            continue;
        }
        else if(type == nsXPCType::T_P_VOID)
        {
            // XXX implement void*
            NS_ASSERTION(0,"void* params not supported");
            continue;
        }
        else {
            int32    ti;
            uint32   tu;
            jsdouble td;
            JSBool   r;

            switch(type & nsXPCType::TYPE_MASK)
            {
            case nsXPCType::T_I8     :
                r = JS_ValueToECMAInt32(cx,src,&ti);
                dp->val.i8  = (int8) ti;
                break;
            case nsXPCType::T_I16    :
                r = JS_ValueToECMAInt32(cx,src,&ti);
                dp->val.i16  = (int16) ti;
                break;
            case nsXPCType::T_I32    :
                r = JS_ValueToECMAInt32(cx,src,&dp->val.i32);
                break;
            case nsXPCType::T_I64    :
                if(JSVAL_IS_INT(src))
                {
                    r = JS_ValueToECMAInt32(cx,src,&ti);
                    dp->val.i64 = (int64) ti;
                }
                else
                {
                    r = JS_ValueToNumber(cx, src, &td);
                    if(r) dp->val.i64 = (int64) td;
                }
                break;
            case nsXPCType::T_U8     :
                r = JS_ValueToECMAUint32(cx,src,&tu);
                dp->val.u8  = (uint8) tu;
                break;
            case nsXPCType::T_U16    :
                r = JS_ValueToECMAUint32(cx,src,&tu);
                dp->val.u16  = (uint16) tu;
                break;
            case nsXPCType::T_U32    :
                r = JS_ValueToECMAUint32(cx,src,&dp->val.u32);
                break;
            case nsXPCType::T_U64    :
                if(JSVAL_IS_INT(src))
                {
                    r = JS_ValueToECMAUint32(cx,src,&tu);
                    dp->val.i64 = (int64) tu;
                }
                else
                {
                    r = JS_ValueToNumber(cx, src, &td);
                    // XXX Win32 can't handle double to uint64 directly
                    if(r) dp->val.u64 = (uint64)((int64) td);
                }
                break;
            case nsXPCType::T_FLOAT  :
                r = JS_ValueToNumber(cx, src, &td);
                if(r) dp->val.f = (float) td;
                break;
            case nsXPCType::T_DOUBLE :
                r = JS_ValueToNumber(cx, src, &dp->val.d);
                break;
            case nsXPCType::T_BOOL   :
                r = JS_ValueToBoolean(cx, src, &dp->val.b);
                break;

            // XXX should we special case char* and wchar_t* to be strings?
            case nsXPCType::T_CHAR   :
            case nsXPCType::T_WCHAR  :
            default:
                NS_ASSERTION(0, "bad type");
                ReportError(desc, "could not convert argument");
                goto done;
            }
            continue;
        }
    }

    // do the invoke
    invokeResult = xpc_InvokeNativeMethod(wrapper->GetNative(), vtblIndex,
                                          paramCount, dispatchParams);

    if(NS_FAILED(invokeResult))
    {
        // XXX this (and others!) should throw rather than report error
        ReportError(desc, "XPCOM object returned failure");
        goto done;
    }

    // iterate through the params to gather the results
    for(i = 0; i < paramCount; i++)
    {
        const nsXPCParamInfo& param = info->GetParam(i);
        const nsXPCType& type = param.GetType();

        nsXPCVarient* dp = &dispatchParams[i];
        if(param.IsOut())
        {
            jsval v;
            if(type == nsXPCType::T_INTERFACE)
            {
                // XXX implement INTERFACE

                // make sure 'src' is an object
                // get the nsIInterfaceInfo* from the param and
                // build a wrapper and then hand over the wrapper.
                // XXX remember to release the wrapper in cleanup below

                NS_ASSERTION(0,"interface params not supported");
                goto done;
                continue;
            }
            else if(type == nsXPCType::T_INTERFACE_IS)
            {
                // XXX implement INTERFACE_IS
                NS_ASSERTION(0,"interface_is params not supported");
                continue;
            }
            else if(type == nsXPCType::T_STRING)
            {
                // XXX implement STRING
                NS_ASSERTION(0,"string params not supported");
                continue;
            }
            else if(type == nsXPCType::T_P_IID)
            {
                // XXX implement IID
                NS_ASSERTION(0,"iid params not supported");
                continue;
            }
            else if(type == nsXPCType::T_P_VOID)
            {
                // XXX implement void*
                NS_ASSERTION(0,"void* params not supported");
                continue;
            }
            else
            {
                jsdouble d;
                switch(type & nsXPCType::TYPE_MASK)
                {
                case nsXPCType::T_I8    :v = INT_TO_JSVAL((int32)dp->val.i8);   break;
                case nsXPCType::T_I16   :v = INT_TO_JSVAL((int32)dp->val.i16);  break;
                case nsXPCType::T_I32   :v = FIT_32(dp->val.i32,d);             break;
                case nsXPCType::T_I64   :v = JAM_DOUBLE(dp->val.i64,d);         break;
                case nsXPCType::T_U8    :v = INT_TO_JSVAL((int32)dp->val.u8);   break;
                case nsXPCType::T_U16   :v = INT_TO_JSVAL((int32)dp->val.u16);  break;
                case nsXPCType::T_U32   :v = FIT_32(dp->val.u32,d);             break;
                case nsXPCType::T_U64   :v = JAM_DOUBLE_U64(dp->val.u64,d);     break;
                case nsXPCType::T_FLOAT :v = JAM_DOUBLE(dp->val.f,d);           break;
                case nsXPCType::T_DOUBLE:v = DOUBLE_TO_JSVAL(&dp->val.d);       break;
                case nsXPCType::T_BOOL  :v = dp->val.b?JSVAL_TRUE:JSVAL_FALSE;  break;
                // XXX should we special case char* and wchar_t* to be strings?
                case nsXPCType::T_CHAR  :/*v = INT_TO_JSVAL((int32)dp->val.c); */   break;
                case nsXPCType::T_WCHAR :/*v = INT_TO_JSVAL((int32)dp->val.wc);*/   break;
                default:
                    // XXX need to support string constants
                    NS_ASSERTION(0, "bad type");
                    ReportError(desc, "invalid out type");
                    goto done;
                }
            }
            if(param.IsRetval())
                *vp = v;
            else
            {
                // we actually assured this before doing the invoke
                NS_ASSERTION(JSVAL_IS_OBJECT(argv[i]), "out var is not object");
                if(!JS_SetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &v))
                {
                    ReportError(desc, "Can't set val on out param object");
                    goto done;
                }
            }
        }
    }
    retval = JS_TRUE;

done:
    // iterate through the params that were init'd (again!) and clean up
    // any alloc'd stuff and release wrappers of params
    for(i = 0; i < dispatchParamsInitedCount; i++)
    {
        nsXPCVarient* dp = &dispatchParams[i];
        void* p = dp->val.p;
        // XXX verify that ALL this this stuff is right
        if(!p)
            continue;
        if(dp->flags & nsXPCVarient::VAL_IS_OWNED)
            delete [] p;
        else if(info->GetParam(i).GetType() == nsXPCType::T_INTERFACE)
            ((nsISupports*)p)->Release();
    }

    if(dispatchParams && dispatchParams != paramBuffer)
        delete [] dispatchParams;
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
        const char* name = GetMemberName(desc);
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
    {
        // XXX silently fail when property not found?
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

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

static JSObjectOps WrappedNative_ops = {
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

static JSClass WrappedNative_class = {
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
    JSContext* cx = GetJSContext();
    JSObject* jsobj = JS_NewObject(cx, &WrappedNative_class, NULL, NULL);
    if(!jsobj || !JS_SetPrivate(cx, jsobj, self))
        return NULL;
    return jsobj;
}

