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

/* Class shared by all native interface instances. */

#include "xpcprivate.h"

/***************************************************************************/
XPCNativeMemberDescriptor::XPCNativeMemberDescriptor()
    : invokeFuncObj(NULL), id(0), index(0), index2(0), flags(0) {}
/***************************************************************************/

const char XPC_VAL_STR[] = "value";

extern "C" JS_IMPORT_DATA(JSObjectOps) js_ObjectOps;

static NS_DEFINE_IID(kWrappedNativeClassIID, NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID);
NS_IMPL_ISUPPORTS(nsXPCWrappedNativeClass, kWrappedNativeClassIID)

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
        nsIInterfaceInfoManager* iimgr;
        if(NULL != (iimgr = nsXPConnect::GetInterfaceInfoManager()))
        {
            nsIInterfaceInfo* info;
            if(NS_SUCCEEDED(iimgr->GetInfoForIID(&aIID, &info)))
            {
                PRBool canScript;
                if(NS_SUCCEEDED(info->IsScriptable(&canScript)) &&
                   canScript &&
                   nsXPConnect::IsISupportsDescendent(info))
                {
                    clazz = new nsXPCWrappedNativeClass(xpcc, aIID, info);
                    if(-1 == clazz->mMemberCount) // -1 means 'failed to init'
                        NS_RELEASE(clazz);  // NULLs out 'clazz'
                }
                NS_RELEASE(info);
            }
            NS_RELEASE(iimgr);
        }
    }
    return clazz;
}

nsXPCWrappedNativeClass::nsXPCWrappedNativeClass(XPCContext* xpcc, REFNSIID aIID,
                                                 nsIInterfaceInfo* aInfo)
    : mXPCContext(xpcc),
      mIID(aIID),
      mName(NULL),
      mInfo(aInfo),
      mMemberCount(-1),
      mDescriptors(NULL)
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
    if(mXPCContext)
        mXPCContext->GetWrappedNativeClassMap()->Remove(this);
    DestroyMemberDescriptors();
    if(mName)
        nsAllocator::Free(mName);
    NS_RELEASE(mInfo);
}

JSBool
nsXPCWrappedNativeClass::BuildMemberDescriptors()
{
    int i;
    uint16 constCount;
    uint16 methodCount;
    uint16 totalCount;
    JSContext* cx = GetJSContext();

    if(!cx ||
       NS_FAILED(mInfo->GetMethodCount(&methodCount))||
       NS_FAILED(mInfo->GetConstantCount(&constCount)))
        return JS_FALSE;

    totalCount = methodCount+constCount;
    if(!totalCount)
    {
        mMemberCount = 0;
        return JS_TRUE;
    }

    // XXX since getters and setters share a descriptor, we might not use all
    // of the objects that get alloc'd here
    mDescriptors = new XPCNativeMemberDescriptor[totalCount];
    if(!mDescriptors)
        return JS_FALSE;
    mMemberCount = 0;

    for(i = 0; i < methodCount; i++)
    {
        jsval idval;
        jsid id;
        XPCNativeMemberDescriptor* desc;
        const nsXPTMethodInfo* info;
        if(NS_FAILED(mInfo->GetMethodInfo(i, &info)))
            return JS_FALSE;

        // don't reflect Addref or Release
        if(i == 1 || i == 2)
            continue;

        if(!XPCConvert::IsMethodReflectable(*info))
            continue;

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
            // XXX ASSUMES Getter/Setter pairs are next to each other
            desc = &mDescriptors[mMemberCount-1];
            NS_ASSERTION(desc->id == id,"bad setter");
            NS_ASSERTION(desc->IsReadOnlyAttribute(),"bad setter");
            desc->SetWritableAttribute();
            desc->index2 = i;
        }
        else
        {
            NS_ASSERTION(!LookupMemberByID(id),"duplicate method name");
            desc = &mDescriptors[mMemberCount++];
            desc->id = id;
            if(info->IsGetter())
                desc->SetReadOnlyAttribute();
            else
                desc->SetMethod();
            desc->index = i;
        }
    }

    for(i = 0; i < constCount; i++)
    {
        jsval idval;
        jsid id;
        XPCNativeMemberDescriptor* desc;
        const nsXPTConstant* constant;
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
        desc = &mDescriptors[mMemberCount++];
        desc->id = id;
        desc->SetConstant();
        desc->index = i;
    }

    return JS_TRUE;
}

void 
nsXPCWrappedNativeClass::XPCContextBeingDestroyed()
{
    DestroyMemberDescriptors(); 
    mXPCContext = NULL;
}

void
nsXPCWrappedNativeClass::DestroyMemberDescriptors()
{
    if(!mDescriptors)
        return;
    JSContext* cx;
    if(NULL != (cx = GetJSContext()))
        for(int i = 0; i < mMemberCount; i++)
            if(mDescriptors[i].invokeFuncObj)
                JS_RemoveRoot(cx, &mDescriptors[i].invokeFuncObj);
    delete [] mDescriptors;
    mDescriptors = NULL;
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
nsXPCWrappedNativeClass::GetMemberName(const XPCNativeMemberDescriptor* desc) const
{
    if(desc->IsConstant())
    {
        const nsXPTConstant* constant;
        if(NS_SUCCEEDED(mInfo->GetConstant(desc->index, &constant)))
            return constant->GetName();
    }
    else // methods or attribute
    {
        const nsXPTMethodInfo* info;
        if(NS_SUCCEEDED(mInfo->GetMethodInfo(desc->index, &info)))
            return info->GetName();
    }
    return NULL;
}

const char*
nsXPCWrappedNativeClass::GetInterfaceName()
{
    if(!mName)
        mInfo->GetName(&mName);
    return mName;
}

JSBool
nsXPCWrappedNativeClass::GetConstantAsJSVal(JSContext *cx,
                                            nsXPCWrappedNative* wrapper,
                                            const XPCNativeMemberDescriptor* desc,
                                            jsval* vp)
{
    const nsXPTConstant* constant;

    NS_ASSERTION(desc->IsConstant(),"bad type");
    if(NS_FAILED(mInfo->GetConstant(desc->index, &constant)))
    {
        // XXX fail silently?
        *vp = JSVAL_NULL;
        return JS_TRUE;
    }
    const nsXPTCMiniVariant& mv = *constant->GetValue();

    // XXX Big Hack!
    nsXPTCVariant v;
    v.flags = 0;
    v.type = constant->GetType();
    memcpy(&v.val, &mv.val, sizeof(mv.val));

    // XXX if iid consts are supported, then coditionally fill it in here
    return XPCConvert::NativeData2JS(cx, vp, &v.val, v.type, NULL, NULL);
}

JSBool
nsXPCWrappedNativeClass::CallWrappedMethod(JSContext* cx,
                                           nsXPCWrappedNative* wrapper,
                                           const XPCNativeMemberDescriptor* desc,
                                           CallMode callMode,
                                           uintN argc, jsval *argv, jsval *vp)
{
#define PARAM_BUFFER_COUNT     16

    nsXPTCVariant paramBuffer[PARAM_BUFFER_COUNT];
    JSBool retval = JS_FALSE;

    nsXPTCVariant* dispatchParams = NULL;
    uint8 i;
    const nsXPTMethodInfo* info;
    uint8 requiredArgs;
    uint8 paramCount;
    uint8 dispatchParamsInitedCount = 0;
    jsval src;
    uint8 vtblIndex;
    nsresult invokeResult;
    nsID* conditional_iid = NULL;
    uintN err;
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    nsIXPCSecurityManager* securityManager;

    if(vp)
        *vp = JSVAL_NULL;

    if(!xpcc)
        goto done;

    xpcc->SetLastResult(NS_ERROR_UNEXPECTED);

    // make sure we have what we need...

    // set up the method index and do the security check if needed
    securityManager = xpcc->GetSecurityManager();
    switch(callMode)
    {
        case CALL_METHOD:
            vtblIndex = desc->index;

            if(securityManager && 
               (xpcc->GetSecurityManagerFlags() & 
                nsIXPCSecurityManager::HOOK_CALL_METHOD) &&
               NS_OK != 
                securityManager->CanCallMethod(cx, mIID, wrapper->GetNative(), 
                                               mInfo, vtblIndex, desc->id))
            {
                // the security manager vetoed. It should have set an exception.
                goto done;
            }
            break;

        case CALL_GETTER:
            vtblIndex = desc->index;

            if(securityManager && 
               (xpcc->GetSecurityManagerFlags() & 
                nsIXPCSecurityManager::HOOK_GET_PROPERTY) &&
               NS_OK != 
                securityManager->CanGetProperty(cx, mIID, wrapper->GetNative(), 
                                                mInfo, vtblIndex, desc->id))
            {
                // the security manager vetoed. It should have set an exception.
                goto done;
            }
            break;

        case CALL_SETTER:
            // fail silently if trying to write a readonly attribute
            if(!desc->IsWritableAttribute())
                goto done;
            vtblIndex = desc->index2;

            if(securityManager && 
               (xpcc->GetSecurityManagerFlags() & 
                nsIXPCSecurityManager::HOOK_SET_PROPERTY) &&
               NS_OK != 
                securityManager->CanSetProperty(cx, mIID, wrapper->GetNative(), 
                                                mInfo, vtblIndex, desc->id))
            {
                // the security manager vetoed. It should have set an exception.
                goto done;
            }
            break;
        default:
            NS_ASSERTION(0,"bad value");
            goto done;
    }

    if(NS_FAILED(mInfo->GetMethodInfo(vtblIndex, &info)))
    {
        ThrowException(XPCJSError::CANT_GET_METHOD_INFO, cx, desc);
        goto done;
    }

    // XXX ASSUMES that retval is last arg.
    paramCount = info->GetParamCount();
    requiredArgs = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);
    if(argc < requiredArgs)
    {
        ThrowException(XPCJSError::NOT_ENOUGH_ARGS, cx, desc);
        goto done;
    }

    // setup variant array pointer
    if(paramCount > PARAM_BUFFER_COUNT)
    {
        if(!(dispatchParams = new nsXPTCVariant[paramCount]))
        {
            JS_ReportOutOfMemory(cx);
            goto done;
        }
    }
    else
        dispatchParams = paramBuffer;

    // iterate through the params doing conversions
    for(i = 0; i < paramCount; i++)
    {
        JSBool useAllocator = JS_FALSE;
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();

        nsXPTCVariant* dp = &dispatchParams[i];
        dp->type = type;
        dp->flags = 0;
        dp->val.p = NULL;
        dispatchParamsInitedCount++;

        if(type.TagPart() == nsXPTType::T_INTERFACE ||
           type.TagPart() == nsXPTType::T_INTERFACE_IS)
        {
            dp->flags |= nsXPTCVariant::VAL_IS_IFACE;
        }

        // set 'src' to be the object from which we get the value and
        // prepare for out param

        if(param.IsOut())
        {
            dp->flags |= nsXPTCVariant::PTR_IS_DATA;
            dp->ptr = &dp->val;

            if(!param.IsRetval() &&
               (JSVAL_IS_PRIMITIVE(argv[i]) ||
                !JS_GetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &src)))
            {
                ThrowBadParamException(XPCJSError::NEED_OUT_OBJECT,
                                       cx, desc, i);
                goto done;
            }
            if(type.IsPointer() && 
               !type.IsInterfacePointer() &&
               !param.IsShared())
            {
                useAllocator = JS_TRUE;
                dp->flags |= nsXPTCVariant::VAL_IS_OWNED;
            }
            if(!param.IsIn())
                continue;
        }
        else
        {
            src = argv[i];
            if(type.IsPointer() && type.TagPart() == nsXPTType::T_IID)
            {
                useAllocator = JS_TRUE;
                dp->flags |= nsXPTCVariant::VAL_IS_OWNED;
            }
        }

        if(type.TagPart() == nsXPTType::T_INTERFACE)
        {
            if(NS_FAILED(GetInterfaceInfo()->
                                GetIIDForParam(&param, &conditional_iid)))
            {
                ThrowBadParamException(XPCJSError::CANT_GET_PARAM_IFACE_INFO,
                                       cx, desc, i);
                goto done;
            }
        }
        else if(type.TagPart() == nsXPTType::T_INTERFACE_IS)
        {
            uint8 arg_num = param.GetInterfaceIsArgNumber();
            const nsXPTParamInfo& param = info->GetParam(arg_num);
            const nsXPTType& type = param.GetType();
            if(!type.IsPointer() || type.TagPart() != nsXPTType::T_IID ||
               !XPCConvert::JSData2Native(cx, &conditional_iid, argv[arg_num],
                                          type, JS_TRUE, NULL, NULL))
            {
                ThrowBadParamException(XPCJSError::CANT_GET_PARAM_IFACE_INFO,
                                       cx, desc, i);
                goto done;
            }
        }

        if(!XPCConvert::JSData2Native(cx, &dp->val, src, type,
                                      useAllocator, conditional_iid, &err))
        {
            ThrowBadParamException(err, cx, desc, i);
            goto done;
        }
        if(conditional_iid)
        {
            nsAllocator::Free((void*)conditional_iid);
            conditional_iid = NULL;
        }
    }

    // do the invoke
    invokeResult = XPTC_InvokeByIndex(wrapper->GetNative(), vtblIndex,
                                      paramCount, dispatchParams);
    xpcc->SetLastResult(invokeResult);
    if(NS_FAILED(invokeResult))
    {
        ThrowBadResultException(cx, desc, invokeResult);
        goto done;
    }

    // iterate through the params to gather the results
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();

        nsXPTCVariant* dp = &dispatchParams[i];
        if(param.IsOut())
        {
            jsval v;

            if(type.TagPart() == nsXPTType::T_INTERFACE)
            {
                if(NS_FAILED(GetInterfaceInfo()->
                                    GetIIDForParam(&param, &conditional_iid)))
                {
                    ThrowBadParamException(XPCJSError::CANT_GET_PARAM_IFACE_INFO,
                                           cx, desc, i);
                    goto done;
                }
            }
            else if(type.TagPart() == nsXPTType::T_INTERFACE_IS)
            {
                uint8 arg_num = param.GetInterfaceIsArgNumber();
                const nsXPTParamInfo& param = info->GetParam(arg_num);
                const nsXPTType& type = param.GetType();
                if(!type.IsPointer() || type.TagPart() != nsXPTType::T_IID ||
                   !(conditional_iid = (nsID*)
                         nsAllocator::Clone(dispatchParams[arg_num].val.p, 
                                          sizeof(nsID))))
                {
                    ThrowBadParamException(XPCJSError::CANT_GET_PARAM_IFACE_INFO,
                                           cx, desc, i);
                    goto done;
                }
            }

            if(!XPCConvert::NativeData2JS(cx, &v, &dp->val, type,
                                          conditional_iid, &err))
            {
                ThrowBadParamException(err, cx, desc, i);
                goto done;
            }

            if(param.IsRetval())
            {
                if(vp)
                    *vp = v;
            }
            else
            {
                // we actually assured this before doing the invoke
                NS_ASSERTION(JSVAL_IS_OBJECT(argv[i]), "out var is not object");
                if(!JS_SetProperty(cx, JSVAL_TO_OBJECT(argv[i]), XPC_VAL_STR, &v))
                {
                    ThrowBadParamException(XPCJSError::CANT_SET_OUT_VAL,
                                           cx, desc, i);
                    goto done;
                }
            }
            if(conditional_iid)
            {
                nsAllocator::Free((void*)conditional_iid);
                conditional_iid = NULL;
            }
        }
    }
    retval = JS_TRUE;

done:
    // iterate through the params that were init'd (again!) and clean up
    // any alloc'd stuff and release wrappers of params
    for(i = 0; i < dispatchParamsInitedCount; i++)
    {
        nsXPTCVariant* dp = &dispatchParams[i];
        void* p = dp->val.p;
        if(!p)
            continue;
        if(dp->IsValOwned())
            nsAllocator::Free(p);
        if(dp->IsValInterface())
            ((nsISupports*)p)->Release();
    }
    if(conditional_iid)
        nsAllocator::Free((void*)conditional_iid);

    if(dispatchParams && dispatchParams != paramBuffer)
        delete [] dispatchParams;
    return retval;
}

JSObject*
nsXPCWrappedNativeClass::GetInvokeFunObj(const XPCNativeMemberDescriptor* desc)
{
    NS_ASSERTION(desc->IsMethod(), "only methods have invoke objects");

    if(!desc->invokeFuncObj)
    {
        const char* name = GetMemberName(desc);
        NS_ASSERTION(name,"bad method name");

        JSContext* cx = GetJSContext();

        if(!cx)
            return NULL;

        const nsXPTMethodInfo* info;
        uint8 requiredArgs = 0;

        if(NS_SUCCEEDED(mInfo->GetMethodInfo(desc->index, &info)))
        {
            // XXX ASSUMES that retval is last arg.
            uint8 paramCount = info->GetParamCount();
            requiredArgs = paramCount -
                (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);
        }

        JSFunction *fun = JS_NewFunction(cx, WrappedNative_CallMethod, 
                                         (uintN) requiredArgs,
                                         JSFUN_BOUND_METHOD, NULL, name);
        if(!fun)
            return NULL;

        XPCNativeMemberDescriptor* descRW =
            NS_CONST_CAST(XPCNativeMemberDescriptor*,desc);

        if(NULL != (descRW->invokeFuncObj = JS_GetFunctionObject(fun)))
            JS_AddNamedRoot(cx, &descRW->invokeFuncObj, 
                            "XPCNativeMemberDescriptor::invokeFuncObj");
    }
    return desc->invokeFuncObj;
}

struct EnumStateHolder
{
    jsval dstate;
    jsval sstate;
};

JSBool
nsXPCWrappedNativeClass::DynamicEnumerate(nsXPCWrappedNative* wrapper,
                                          nsIXPCScriptable* ds,
                                          nsIXPCScriptable* as,
                                          JSContext *cx, JSObject *obj,
                                          JSIterateOp enum_op,
                                          jsval *statep, jsid *idp)
{
    EnumStateHolder* holder;

    switch(enum_op) {
    case JSENUMERATE_INIT:
    {
        if(NULL != (holder = new EnumStateHolder()))
        {
            jsid  did;
            jsid  sid;
            JSBool retval;
            if(NS_FAILED(ds->Enumerate(cx, obj, JSENUMERATE_INIT,
                                &holder->dstate, &did, wrapper,
                                as, &retval)) || !retval)
            {
                holder->dstate = JSVAL_NULL;
                did = INT_TO_JSVAL(0);
            }
            // we *know* this won't fail
            StaticEnumerate(wrapper, JSENUMERATE_INIT, &holder->sstate, &sid);
            int total = JSVAL_TO_INT(did) + JSVAL_TO_INT(sid);
            if(total)
            {
                if (idp)
                    *idp = INT_TO_JSVAL(total);
                *statep = PRIVATE_TO_JSVAL(holder);
                return JS_TRUE;
            }
            else
                delete holder;
        }
        *statep = JSVAL_NULL;
        return JS_TRUE;
    }
    case JSENUMERATE_NEXT:
        holder = (EnumStateHolder*) JSVAL_TO_PRIVATE(*statep);
        NS_ASSERTION(holder,"bad statep");

        if(holder->dstate != JSVAL_NULL)
        {
            JSBool retval;
            if(NS_FAILED(ds->Enumerate(cx, obj, JSENUMERATE_NEXT,
                                &holder->dstate, idp, wrapper,
                                as, &retval)) || !retval)
                *idp = holder->dstate = JSVAL_NULL;
        }

        if(holder->dstate == JSVAL_NULL && holder->sstate != JSVAL_NULL)
            StaticEnumerate(wrapper, JSENUMERATE_NEXT, &holder->sstate, idp);

        // are we done?
        if(holder->dstate != JSVAL_NULL || holder->sstate != JSVAL_NULL)
            return JS_TRUE;

        /* Fall through ... */

    case JSENUMERATE_DESTROY:
        if(NULL != (holder = (EnumStateHolder*) JSVAL_TO_PRIVATE(*statep)))
        {
            JSBool retval;
            if(holder->dstate != JSVAL_NULL)
                ds->Enumerate(cx, obj, JSENUMERATE_DESTROY,
                              &holder->dstate, idp, wrapper,
                              as, &retval);
            if(holder->sstate != JSVAL_NULL)
                StaticEnumerate(wrapper, JSENUMERATE_DESTROY, &holder->sstate, idp);
            delete holder;
        }
        *statep = JSVAL_NULL;
        return JS_TRUE;

    default:
        NS_ASSERTION(0,"bad enum_op");
        return JS_FALSE;
    }
}

JSBool
nsXPCWrappedNativeClass::StaticEnumerate(nsXPCWrappedNative* wrapper,
                                         JSIterateOp enum_op,
                                         jsval *statep, jsid *idp)
{
    switch(enum_op) {
    case JSENUMERATE_INIT:
        *statep = INT_TO_JSVAL(0);
        if (idp)
            *idp = INT_TO_JSVAL(GetMemberCount());
        return JS_TRUE;

    case JSENUMERATE_NEXT:
    {
        int index = JSVAL_TO_INT(*statep);
        int count = GetMemberCount();

        if (index < count) {
            *idp = GetMemberDescriptor(index)->id;
            *statep =  INT_TO_JSVAL(index+1);
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

// static
JSBool
nsXPCWrappedNativeClass::OneTimeInit()
{
    return xpc_WrappedNativeJSOpsOneTimeInit();
}

// static
JSBool
nsXPCWrappedNativeClass::InitForContext(XPCContext* xpcc)
{
    return JS_TRUE;
}

JSObject*
nsXPCWrappedNativeClass::NewInstanceJSObject(nsXPCWrappedNative* self)
{
    JSContext* cx = GetJSContext();
    if(!cx)
        return NULL;
    JSClass* jsclazz = self->GetDynamicScriptable() ? 
                            &WrappedNativeWithCall_class :
                            &WrappedNative_class;
    JSObject* jsobj = JS_NewObject(cx, jsclazz, NULL, JS_GetGlobalObject(cx));
    if(!jsobj || !JS_SetPrivate(cx, jsobj, self))
        return NULL;
    // wrapper is responsible for calling DynamicScriptable->Create
    return jsobj;
}

// static
nsXPCWrappedNative*
nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(JSContext* cx,
                                                    JSObject* jsobj)
{
    NS_PRECONDITION(jsobj, "bad param");
    if(jsobj && (JS_InstanceOf(cx, jsobj, &WrappedNative_class, NULL) ||
                 JS_InstanceOf(cx, jsobj, &WrappedNativeWithCall_class, NULL)))
        return (nsXPCWrappedNative*) JS_GetPrivate(cx, jsobj);
    return NULL;
}


/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedNativeClass::DebugDump(int depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPCWrappedNativeClass @ %x with mRefCnt = %d", this, mRefCnt));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("interface name is %s", GetInterfaceName()));
        char * iid = mIID.ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid));
        delete iid;
        XPC_LOG_ALWAYS(("InterfaceInfo @ %x", mInfo));
        if(depth)
        {
            uint16 i;
            nsIInterfaceInfo* parent;
            XPC_LOG_INDENT();
            mInfo->GetParent(&parent);
            XPC_LOG_ALWAYS(("parent @ %x", parent));
            mInfo->GetMethodCount(&i);
            XPC_LOG_ALWAYS(("MethodCount = %d", i));
            mInfo->GetConstantCount(&i);
            XPC_LOG_ALWAYS(("ConstantCount = %d", i));
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_ALWAYS(("mXPCContext @ %x", mXPCContext));
        XPC_LOG_ALWAYS(("JSContext @ %x", GetJSContext()));
        XPC_LOG_ALWAYS(("mDescriptors @ %x count = %d", mDescriptors, mMemberCount));
        if(depth && mDescriptors)
        {
            depth--;
            XPC_LOG_INDENT();
            for(int i = 0; i < mMemberCount; i++)
            {
                const XPCNativeMemberDescriptor& desc = mDescriptors[i];
                XPC_LOG_ALWAYS(("Descriptor @ %x", &desc));
                if(depth)
                {
                    depth--;
                    XPC_LOG_INDENT();
                    XPC_LOG_ALWAYS(("category: %s %s %s %s", \
                        desc.IsConstant() ? "Constant" : "", \
                        desc.IsMethod()   ? "Method" : "", \
                        desc.IsWritableAttribute() ? "WritableAttribute" : "", \
                        desc.IsReadOnlyAttribute() ? "ReadOnlyAttribute" : ""));
                    XPC_LOG_ALWAYS(("id is %x", desc.id));
                    if(depth)
                    {
                        XPC_LOG_INDENT();
                        jsval idval;
                        const char *name;
                        if (JS_IdToValue(GetJSContext(), desc.id, &idval) &&
                            JSVAL_IS_STRING(idval) &&
                           (name = JS_GetStringBytes(
                                    JSVAL_TO_STRING(idval))) != NULL)
                        {
                            XPC_LOG_ALWAYS(("property name is %s", name));
                        }
                        XPC_LOG_OUTDENT();
                    }
                    XPC_LOG_ALWAYS(("index is %d", desc.index));
                    XPC_LOG_ALWAYS(("index2 is %d", desc.index2));
                    XPC_LOG_ALWAYS(("invokeFuncObj @ %x", desc.invokeFuncObj));
                    XPC_LOG_OUTDENT();
                    depth++;
                }

            }
            XPC_LOG_OUTDENT();
            depth++;
        }
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}

