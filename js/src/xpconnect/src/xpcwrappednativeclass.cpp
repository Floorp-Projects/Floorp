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
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/* Class shared by all native interface instances. */

#include "xpcprivate.h"

// To kill #define index(a,b) strchr(a,b) macro in Toolkit types.h
#ifdef XP_OS2_VACPP
#if defined index
#undef index
//#define index index
#endif
#endif


/***************************************************************************/
XPCNativeMemberDescriptor::XPCNativeMemberDescriptor()
    : id(0), index(0), index2(0), argc(-1), flags(0) {}
/***************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPCWrappedNativeClass, nsIXPCWrappedNativeClass)

#define SET_ERROR_CODE(_y) if(pErr) *pErr = _y

// static
nsXPCWrappedNativeClass*
nsXPCWrappedNativeClass::GetNewOrUsedClass(XPCContext* xpcc,
                                           REFNSIID aIID,
                                           nsresult* pErr)
{
    nsXPCWrappedNativeClass* clazz = nsnull;
    XPCJSRuntime* rt;

    if(!xpcc || !(rt = xpcc->GetRuntime()))
    {
        SET_ERROR_CODE(NS_ERROR_FAILURE);
        return nsnull;
    }

    SET_ERROR_CODE(NS_OK);

    {   // scoped lock
        nsAutoLock lock(rt->GetMapLock());  
        IID2WrappedNativeClassMap* map = rt->GetWrappedNativeClassMap();
        clazz = map->Find(aIID);
    }

    if(clazz)
    {
        NS_ADDREF(clazz);
        return clazz;
    }

    nsCOMPtr<nsIInterfaceInfoManager> iimgr = 
                dont_AddRef(nsXPConnect::GetInterfaceInfoManager());
    if(!iimgr)
    {
        SET_ERROR_CODE(NS_ERROR_FAILURE);
        return nsnull;
    }

    nsCOMPtr<nsIInterfaceInfo> info;
    if(NS_FAILED(iimgr->GetInfoForIID(&aIID, getter_AddRefs(info))))
    {
        SET_ERROR_CODE(NS_ERROR_XPC_CANT_GET_INTERFACE_INFO);
        return nsnull;
    }

    PRBool canScript;
    if(NS_FAILED(info->IsScriptable(&canScript)))
    {
        SET_ERROR_CODE(NS_ERROR_FAILURE);
        return nsnull;
    }

    if(!canScript)
    {
        SET_ERROR_CODE(NS_ERROR_XPC_INTERFACE_NOT_SCRIPTABLE);
        return nsnull;
    }

    if(!nsXPConnect::IsISupportsDescendant(info))
    {
        SET_ERROR_CODE(NS_ERROR_XPC_INTERFACE_NOT_FROM_NSISUPPORTS);
        return nsnull;
    }

    clazz = new nsXPCWrappedNativeClass(xpcc, aIID, info);
    if(-1 == clazz->mMemberCount) // -1 means 'failed to init'
    {
        SET_ERROR_CODE(NS_ERROR_FAILURE);
        NS_RELEASE(clazz);  // nsnulls out 'clazz'
    }
    return clazz;
}

nsXPCWrappedNativeClass::nsXPCWrappedNativeClass(XPCContext* xpcc,
                                                 REFNSIID aIID,
                                                 nsIInterfaceInfo* aInfo)
    : mRuntime(xpcc->GetRuntime()),
      mIID(aIID),
      mName(nsnull),
      mInfo(aInfo),
      mMemberCount(-1),
      mDescriptors(nsnull)
{
    NS_ADDREF(mInfo);

    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    {   // scoped lock
        nsAutoLock lock(mRuntime->GetMapLock());  
        mRuntime->GetWrappedNativeClassMap()->Add(this);
    }

    if(!BuildMemberDescriptors(xpcc))
        mMemberCount = -1;
}

nsXPCWrappedNativeClass::~nsXPCWrappedNativeClass()
{
    {   // scoped lock
        nsAutoLock lock(mRuntime->GetMapLock());  
        mRuntime->GetWrappedNativeClassMap()->Remove(this);
    }
    DestroyMemberDescriptors();
    if(mName)
        nsMemory::Free(mName);
    NS_RELEASE(mInfo);
}

JSBool
nsXPCWrappedNativeClass::BuildMemberDescriptors(XPCContext* xpcc)
{
    int i;
    uint16 constCount;
    uint16 methodCount;
    uint16 totalCount;
    JSContext* cx = xpcc->GetJSContext();

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

/*
void
nsXPCWrappedNativeClass::XPCContextBeingDestroyed()
{
    DestroyMemberDescriptors();
    mXPCContext = nsnull;
}
*/

void
nsXPCWrappedNativeClass::DestroyMemberDescriptors()
{
    if(!mDescriptors)
        return;
    delete [] mDescriptors;
    mDescriptors = nsnull;
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
    return nsnull;
}

#ifdef XPC_DETECT_LEADING_UPPERCASE_ACCESS_ERRORS
void 
nsXPCWrappedNativeClass::HandlePossibleNameCaseError(JSContext* cx, jsid id)
{
    jsval val;
    JSString* oldJSStr;
    JSString* newJSStr;
    PRUnichar* oldStr;
    PRUnichar* newStr;
    jsid newID;
    const XPCNativeMemberDescriptor* desc;

    if(!cx)
        return;

    if(JS_IdToValue(cx, id, &val) &&
       JSVAL_IS_STRING(val) &&
       nsnull != (oldJSStr = JSVAL_TO_STRING(val)) &&
       nsnull != (oldStr = (PRUnichar*) JS_GetStringChars(oldJSStr)) &&
       oldStr[0] != 0 &&
       nsCRT::IsUpper(oldStr[0]) &&
       nsnull != (newStr = nsCRT::strdup(oldStr)))
    {
        newStr[0] = nsCRT::ToLower(newStr[0]);
        newJSStr = JS_NewUCStringCopyZ(cx, (const jschar*)newStr);
        nsCRT::free(newStr);
        if(newJSStr && 
           JS_ValueToId(cx, STRING_TO_JSVAL(newJSStr), &newID) && 
           newID &&
           nsnull != (desc = LookupMemberByID(newID)))
        {
            // found it!
            const char* ifaceName = GetInterfaceName();
            const char* goodName = JS_GetStringBytes(newJSStr);
            const char* badName = JS_GetStringBytes(oldJSStr);
            char* locationStr = nsnull;

            nsCOMPtr<nsXPCException> e =
                dont_AddRef(nsXPCException::NewException("", NS_OK, 
                                                         nsnull, nsnull));

            nsCOMPtr<nsIJSStackFrameLocation> loc = nsnull;
            if(e)
            {
                nsresult rv;
                rv = e->GetLocation(getter_AddRefs(loc));
                if(NS_SUCCEEDED(rv) && loc)
                {
                    rv = loc->ToString(&locationStr);
                    if(NS_FAILED(rv))
                        locationStr = nsnull;
                }
            }
                        
            if(locationStr && ifaceName && goodName && badName )
            {
                printf("**************************************************\n"
                       "ERROR: JS code at [%s]\n"
                       "tried to access nonexistent property called\n"
                       "\'%s\' on interface of type \'%s\'.\n"
                       "That interface does however have a property called\n"
                       "\'%s\'. Did you mean to access that lowercase property?\n"
                       "Please fix the JS code as appropriate.\n"
                       "**************************************************\n",
                        locationStr, badName, ifaceName, goodName);
            }
            if(locationStr)
                nsMemory::Free(locationStr);
        }
    }
}        
#endif

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
    return nsnull;
}

const char*
nsXPCWrappedNativeClass::GetInterfaceName()
{
    if(!mName)
        mInfo->GetName(&mName);
    return mName;
}

// static
JSBool
nsXPCWrappedNativeClass::GetConstantAsJSVal(JSContext *cx,
                                            nsIInterfaceInfo* iinfo,
                                            PRUint16 index,
                                            jsval* vp)
{
    const nsXPTConstant* constant;

    if(NS_FAILED(iinfo->GetConstant(index, &constant)))
        return JS_FALSE;

    const nsXPTCMiniVariant& mv = *constant->GetValue();

    // XXX Big Hack!
    nsXPTCVariant v;
    v.flags = 0;
    v.type = constant->GetType();
    memcpy(&v.val, &mv.val, sizeof(mv.val));

    return XPCConvert::NativeData2JS(cx, vp, &v.val, v.type, 
                                     nsnull, nsnull, nsnull);
}

JSBool
nsXPCWrappedNativeClass::GetArraySizeFromParam(
                                    JSContext* cx,
                                    const nsXPTMethodInfo* method,
                                    const XPCNativeMemberDescriptor* desc,
                                    const nsXPTParamInfo& param,
                                    uint16 vtblIndex,
                                    uint8 paramIndex,
                                    SizeMode mode,
                                    nsXPTCVariant* dispatchParams,
                                    JSUint32* result)
{
    uint8 argnum;
    nsresult rv;

    // XXX fixup the various exceptions that are thrown

    if(mode == GET_SIZE)
        rv = mInfo->GetSizeIsArgNumberForParam(vtblIndex, &param, 0, &argnum);
    else
        rv = mInfo->GetLengthIsArgNumberForParam(vtblIndex, &param, 0, &argnum);
    if(NS_FAILED(rv))
    {
        ThrowException(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, cx, desc);
        return JS_FALSE;
    }

    const nsXPTParamInfo& arg_param = method->GetParam(argnum);
    const nsXPTType& arg_type = arg_param.GetType();

    // XXX require PRUint32 here - need to require in compiler too!
    if(arg_type.IsPointer() || arg_type.TagPart() != nsXPTType::T_U32)
    {
        ThrowException(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, cx, desc);
        return JS_FALSE;
    }

    *result = dispatchParams[argnum].val.u32;

    return JS_TRUE;
}


JSBool
nsXPCWrappedNativeClass::GetInterfaceTypeFromParam(
                                    JSContext* cx,
                                    const nsXPTMethodInfo* method,
                                    const XPCNativeMemberDescriptor* desc,
                                    const nsXPTParamInfo& param,
                                    uint16 vtblIndex,
                                    uint8 paramIndex,
                                    const nsXPTType& datum_type,
                                    nsXPTCVariant* dispatchParams,
                                    nsID** result)
{
    uint8 argnum;
    nsresult rv;
    uint8 type_tag = datum_type.TagPart();

    // XXX fixup the various exceptions that are thrown

    if(type_tag == nsXPTType::T_INTERFACE)
    {
        rv = GetInterfaceInfo()->GetIIDForParam(vtblIndex, &param, result);
        if(NS_FAILED(rv))
        {
            ThrowBadParamException(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO,
                                   cx, desc, paramIndex);
            return JS_FALSE;
        }
    }
    else if(type_tag == nsXPTType::T_INTERFACE_IS)
    {
        rv = mInfo->GetInterfaceIsArgNumberForParam(vtblIndex, &param, &argnum);
        if(NS_FAILED(rv))
        {
            ThrowException(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, cx, desc);
            return JS_FALSE;
        }

        const nsXPTParamInfo& arg_param = method->GetParam(argnum);
        const nsXPTType& arg_type = arg_param.GetType();
        // XXX require iid type here - need to require in compiler too!
        if(!arg_type.IsPointer() || arg_type.TagPart() != nsXPTType::T_IID)
        {
            ThrowBadParamException(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO,
                                   cx, desc, paramIndex);
            return JS_FALSE;
        }

        if(!(*result = (nsID*) nsMemory::Clone(dispatchParams[argnum].val.p,
                                                  sizeof(nsID))))
        {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

JSBool
nsXPCWrappedNativeClass::CallWrappedMethod(JSContext* cx,
                                           nsXPCWrappedNative* wrapper,
                                           const XPCNativeMemberDescriptor* desc,
                                           CallMode callMode,
                                           uintN argc, jsval *argv, jsval *vp)
{
#define PARAM_BUFFER_COUNT     8

    nsXPTCVariant paramBuffer[PARAM_BUFFER_COUNT];
    JSBool retval = JS_FALSE;

    nsXPTCVariant* dispatchParams = nsnull;
    uint8 i;
    const nsXPTMethodInfo* info;
    uint8 requiredArgs;
    uint8 paramCount;
    jsval src;
    uint16 vtblIndex;
    nsresult invokeResult;
    nsID* conditional_iid = nsnull;
    uintN err;
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    nsIXPCSecurityManager* sm;
    JSBool foundDependentParam;
    nsISupports* callee = wrapper->GetNative();
    NativeCallContextData ccdata;
    NativeCallContextData* oldccdata;
    nsXPCNativeCallContext* cc;
    XPCJSRuntime* rt;
    nsXPConnect* xpc;

#ifdef DEBUG_stats_jband
    PRIntervalTime startTime = PR_IntervalNow();
    PRIntervalTime endTime = 0;
    static int totalTime = 0;
    static int count = 0;
    static const int interval = 10;
    if(0 == (++count % interval))
        printf(">>>>>>>> %d calls on nsXPCWrappedNatives made.  (%d)\n", count, PR_IntervalToMilliseconds(totalTime));
#endif

    if(vp)
        *vp = JSVAL_NULL;

    if(!xpcc || !(rt = xpcc->GetRuntime()) || !(xpc = rt->GetXPConnect()))
        goto done;

    xpc->SetPendingException(nsnull);
    xpcc->SetLastResult(NS_ERROR_UNEXPECTED);

    // make sure we have what we need...

    // set up the method index and do the security check if needed
    NS_WARN_IF_FALSE(xpcc->CallerTypeIsKnown(),"missing caller type set somewhere");
    switch(callMode)
    {
        case CALL_METHOD:
            vtblIndex = desc->index;

            sm = xpcc->GetAppropriateSecurityManager(
                                nsIXPCSecurityManager::HOOK_CALL_METHOD);
            if(sm && NS_OK != sm->CanCallMethod(cx, mIID, callee,
                                                mInfo, vtblIndex, desc->id))
            {
                // the security manager vetoed. It should have set an exception.
                goto done;
            }
            break;

        case CALL_GETTER:
            vtblIndex = desc->index;

            sm = xpcc->GetAppropriateSecurityManager(
                                nsIXPCSecurityManager::HOOK_GET_PROPERTY);
            if(sm && NS_OK != sm->CanGetProperty(cx, mIID, callee,
                                                 mInfo, vtblIndex, desc->id))
            {
                // the security manager vetoed. It should have set an exception.
                goto done;
            }
            break;

        case CALL_SETTER:
            // XXX fail silently if trying to write a readonly attribute
            // XXX throw?
            if(!desc->IsWritableAttribute())
                goto done;
            vtblIndex = desc->index2;

            sm = xpcc->GetAppropriateSecurityManager(
                                nsIXPCSecurityManager::HOOK_SET_PROPERTY);
            if(sm && NS_OK != sm->CanSetProperty(cx, mIID, callee,
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
        ThrowException(NS_ERROR_XPC_CANT_GET_METHOD_INFO, cx, desc);
        goto done;
    }

    // XXX ASSUMES that retval is last arg.
    paramCount = info->GetParamCount();
    requiredArgs = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);
    if(argc < requiredArgs)
    {
        ThrowException(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, desc);
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

    // iterate through the params to clear flags (for safe cleanup later)
    for(i = 0; i < paramCount; i++)
    {
        nsXPTCVariant* dp = &dispatchParams[i];
        dp->flags = 0;
        dp->val.p = nsnull;
    }

    // Iterate through the params doing conversions of independent params only.
    // When we later convert the dependent params (if any) we will know that 
    // the params upon which they depend will have already been converted - 
    // regardless of ordering.
    foundDependentParam = JS_FALSE;
    for(i = 0; i < paramCount; i++)
    {
        JSBool useAllocator = JS_FALSE;
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        uint8 type_tag = type.TagPart();

        if(type.IsDependent())
        {
            foundDependentParam = JS_TRUE;
            continue;
        }

        nsXPTCVariant* dp = &dispatchParams[i];
        dp->type = type;

        if(type_tag == nsXPTType::T_INTERFACE)
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
                !OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(argv[i]),
                                  rt->GetStringID(XPCJSRuntime::IDX_VALUE),
                                  &src)))
            {
                ThrowBadParamException(NS_ERROR_XPC_NEED_OUT_OBJECT,
                                       cx, desc, i);
                goto done;
            }

            if(type.IsPointer() &&
               type_tag != nsXPTType::T_INTERFACE &&
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

            if(type.IsPointer() &&
               type_tag == nsXPTType::T_IID)
            {
                useAllocator = JS_TRUE;
                dp->flags |= nsXPTCVariant::VAL_IS_OWNED;
            }
        }

        if(type_tag == nsXPTType::T_INTERFACE &&
           NS_FAILED(GetInterfaceInfo()->GetIIDForParam(vtblIndex, &param,
                                                        &conditional_iid)))
        {
            ThrowBadParamException(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO,
                                   cx, desc, i);
            goto done;
        }

        if(!XPCConvert::JSData2Native(cx, &dp->val, src, type,
                                      useAllocator, conditional_iid, &err))
        {
            ThrowBadParamException(err, cx, desc, i);
            goto done;
        }

        if(conditional_iid)
        {
            nsMemory::Free((void*)conditional_iid);
            conditional_iid = nsnull;
        }
    }

    // if any params were dependent, then we must iterate again to convert them.
    if(foundDependentParam)
    {
        for(i = 0; i < paramCount; i++)
        {
            const nsXPTParamInfo& param = info->GetParam(i);
            const nsXPTType& type = param.GetType();

            if(!type.IsDependent())
                continue;

            nsXPTType datum_type;
            JSUint32 array_count;
            JSUint32 array_capacity;
            JSBool useAllocator = JS_FALSE;
            PRBool isArray = type.IsArray();

            PRBool isSizedString = isArray ? 
                    JS_FALSE :
                    type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
                    type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;

            nsXPTCVariant* dp = &dispatchParams[i];
            dp->type = type;

            if(isArray)
            {
                dp->flags |= nsXPTCVariant::VAL_IS_ARRAY;

                if(NS_FAILED(mInfo->GetTypeForParam(vtblIndex, &param, 1,
                                                    &datum_type)))
                {
                    ThrowException(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, cx, desc);
                    goto done;
                }
            }
            else
                datum_type = type;

            if(datum_type.IsInterfacePointer())
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
                    !OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(argv[i]),
                        rt->GetStringID(XPCJSRuntime::IDX_VALUE), &src)))
                {
                    ThrowBadParamException(NS_ERROR_XPC_NEED_OUT_OBJECT,
                                           cx, desc, i);
                    goto done;
                }

                if(datum_type.IsPointer() &&
                   !datum_type.IsInterfacePointer() &&
                   (isArray || !param.IsShared()))
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

                if(datum_type.IsPointer() &&
                   datum_type.TagPart() == nsXPTType::T_IID)
                {
                    useAllocator = JS_TRUE;
                    dp->flags |= nsXPTCVariant::VAL_IS_OWNED;
                }
            }

            if(datum_type.IsInterfacePointer() &&
               !GetInterfaceTypeFromParam(cx, info, desc, param, vtblIndex,
                                          i, datum_type, dispatchParams,
                                          &conditional_iid))
                goto done;

            if(isArray || isSizedString)
            {
                if(!GetArraySizeFromParam(cx, info, desc, param, vtblIndex, i,
                                          GET_SIZE, dispatchParams, 
                                          &array_capacity)||
                   !GetArraySizeFromParam(cx, info, desc, param, vtblIndex, i,
                                          GET_LENGTH, dispatchParams, 
                                          &array_count))
                    goto done;

                if(isArray)
                {
                    if(!XPCConvert::JSArray2Native(cx, (void**)&dp->val, src,
                                                   array_count, array_capacity,
                                                   datum_type,
                                                   useAllocator, 
                                                   conditional_iid, &err))
                    {
                        // XXX need exception scheme for arrays to indicate bad element
                        ThrowBadParamException(err, cx, desc, i);
                        goto done;
                    }
                }
                else // if(isSizedString)
                {
                    if(!XPCConvert::JSStringWithSize2Native(cx, (void*)&dp->val, 
                                                   src,
                                                   array_count, array_capacity,
                                                   datum_type, useAllocator, 
                                                   &err))
                    {
                        ThrowBadParamException(err, cx, desc, i);
                        goto done;
                    }
                }
            }
            else
            {
                if(!XPCConvert::JSData2Native(cx, &dp->val, src, type,
                                              useAllocator, conditional_iid, 
                                              &err))
                {
                    ThrowBadParamException(err, cx, desc, i);
                    goto done;
                }
            }

            if(conditional_iid)
            {
                nsMemory::Free((void*)conditional_iid);
                conditional_iid = nsnull;
            }
        }
    }

    // setup the call context in case the callee wants to see it
    cc = xpcc->GetNativeCallContext(); // can't fail
    ccdata.init(callee, vtblIndex, wrapper, cx, argc, argv, vp);
    oldccdata = cc->SetData(&ccdata);

    // do the invoke
    invokeResult = XPTC_InvokeByIndex(callee, vtblIndex,
                                      paramCount, dispatchParams);
    
    xpcc->SetLastResult(invokeResult);
    cc->SetData(oldccdata);

    if(NS_FAILED(invokeResult))
    {
        ThrowBadResultException(cx, desc, invokeResult);
        goto done;
    }
    else if(ccdata.threw)
    {
        // the native callee claims to have already set a JSException
        goto done;
    }

    // now we iterate through the native params to gather and convert results
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        if(!param.IsOut())
            continue;

        const nsXPTType& type = param.GetType();
        nsXPTCVariant* dp = &dispatchParams[i];
        jsval v;
        JSUint32 array_count;
        nsXPTType datum_type;
        PRBool isArray = type.IsArray();
        PRBool isSizedString = isArray ? 
                JS_FALSE :
                type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
                type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;

        if(isArray)
        {
            if(NS_FAILED(mInfo->GetTypeForParam(vtblIndex, &param, 1,
                                                &datum_type)))
            {
                ThrowException(NS_ERROR_XPC_CANT_GET_ARRAY_INFO, cx, desc);
                goto done;
            }
        }
        else
            datum_type = type;

        if(isArray || isSizedString)
        {
            if(!GetArraySizeFromParam(cx, info, desc, param, vtblIndex, i,
                                      GET_LENGTH, dispatchParams,
                                      &array_count))
                goto done;
        }

        if(datum_type.IsInterfacePointer() &&
           !GetInterfaceTypeFromParam(cx, info, desc, param, vtblIndex,
                                      i, datum_type, dispatchParams,
                                      &conditional_iid))
            goto done;

        if(isArray)
        {
            if(!XPCConvert::NativeArray2JS(cx, &v, (const void**)&dp->val,
                                           datum_type, conditional_iid,
                                           array_count, wrapper->GetJSObject(),
                                           &err))
            {
                // XXX need exception scheme for arrays to indicate bad element
                ThrowBadParamException(err, cx, desc, i);
                goto done;
            }
        }
        else if (isSizedString)
        {
            if(!XPCConvert::NativeStringWithSize2JS(cx, &v, 
                                           (const void*)&dp->val,
                                           datum_type,
                                           array_count, &err))
            {
                ThrowBadParamException(err, cx, desc, i);
                goto done;
            }
        }
        else
        {
            if(!XPCConvert::NativeData2JS(cx, &v, &dp->val, datum_type,
                                          conditional_iid, 
                                          wrapper->GetJSObject(), &err))
            {
                ThrowBadParamException(err, cx, desc, i);
                goto done;
            }
        }

        if(param.IsRetval())
        {
            if(vp && !ccdata.retvalSet)
                *vp = v;
        }
        else
        {
            // we actually assured this before doing the invoke
            NS_ASSERTION(JSVAL_IS_OBJECT(argv[i]), "out var is not object");
            if(!OBJ_SET_PROPERTY(cx, JSVAL_TO_OBJECT(argv[i]),
                        rt->GetStringID(XPCJSRuntime::IDX_VALUE), &v))
            {
                ThrowBadParamException(NS_ERROR_XPC_CANT_SET_OUT_VAL,
                                       cx, desc, i);
                goto done;
            }
        }
        if(conditional_iid)
        {
            nsMemory::Free((void*)conditional_iid);
            conditional_iid = nsnull;
        }
    }

    retval = JS_TRUE;
done:
    // iterate through the params (again!) and clean up
    // any alloc'd stuff and release wrappers of params
    if(dispatchParams)
    {
        for(i = 0; i < paramCount; i++)
        {
            nsXPTCVariant* dp = &dispatchParams[i];
            void* p = dp->val.p;
            if(!p)
                continue;

            if(dp->IsValArray())
            {
                // going to have to cleanup the array and perhaps its contents
                if(dp->IsValOwned() || dp->IsValInterface())
                {
                    // we need to figure out how many elements are present.
                    JSUint32 array_count;

                    const nsXPTParamInfo& param = info->GetParam(i);
                    if(!GetArraySizeFromParam(cx, info, desc, param, vtblIndex,
                                              i, GET_LENGTH, dispatchParams,
                                              &array_count))
                    {
                        NS_ASSERTION(0,"failed to get array length, we'll leak here");
                        continue;
                    }
                    if(dp->IsValOwned())
                    {
                        void** a = (void**)p;
                        for(JSUint32 k = 0; k < array_count; k++)
                        {
                            void* o = a[k];
                            if(o) nsMemory::Free(o);
                        }
                    }
                    else // if(dp->IsValInterface())
                    {
                        nsISupports** a = (nsISupports**)p;
                        for(JSUint32 k = 0; k < array_count; k++)
                        {
                            nsISupports* o = a[k];
                            NS_IF_RELEASE(o);
                        }
                    }
                }
                // always free the array itself
                nsMemory::Free(p);
            }
            else if(dp->IsValOwned())
                nsMemory::Free(p);
            else if(dp->IsValInterface())
                ((nsISupports*)p)->Release();
        }
    }

    if(conditional_iid)
        nsMemory::Free((void*)conditional_iid);

    if(dispatchParams && dispatchParams != paramBuffer)
        delete [] dispatchParams;

#ifdef DEBUG_stats_jband
    endTime = PR_IntervalNow();
    
    printf("%s::%s %d ( js->c ) \n", GetInterfaceName(), GetMemberName(desc), PR_IntervalToMilliseconds(endTime-startTime));

    totalTime += (endTime-startTime);
#endif

    return retval;
}

JSObject*
nsXPCWrappedNativeClass::NewFunObj(JSContext *cx, JSObject *obj,
                                   const XPCNativeMemberDescriptor* desc)
{
    NS_ASSERTION(desc->IsMethod(), "we can only create FunObjs for methods");

    if(-1 == desc->argc)
    {
        const nsXPTMethodInfo* info;
        if(NS_SUCCEEDED(mInfo->GetMethodInfo(desc->index, &info)))
        {
            // XXX ASSUMES that retval is last arg.
            uint8 paramCount = info->GetParamCount();
            XPCNativeMemberDescriptor* descRW =
                NS_CONST_CAST(XPCNativeMemberDescriptor*,desc);
            descRW->argc = ((intN)paramCount) -
                (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);
        }
        else
            return nsnull;
    }

    JSFunction *fun = JS_NewFunction(cx, WrappedNative_CallMethod,
                                     (uintN) desc->argc,
                                     JSFUN_BOUND_METHOD, obj, 
                                     GetMemberName(desc));
    if(!fun)
        return nsnull;
    return JS_GetFunctionObject(fun);
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
        if(nsnull != (holder = new EnumStateHolder()))
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
        if(nsnull != (holder = (EnumStateHolder*) JSVAL_TO_PRIVATE(*statep)))
        {
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
        }

        /* Fall through ... */

    case JSENUMERATE_DESTROY:
        if(nsnull != (holder = (EnumStateHolder*) JSVAL_TO_PRIVATE(*statep)))
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
    int totalCount = (wrapper->GetDynamicScriptableFlags() &
                      XPCSCRIPTABLE_DONT_ENUM_STATIC_PROPS) ?
                        0 : GetMemberCount();

    switch(enum_op) {
    case JSENUMERATE_INIT:
        *statep = INT_TO_JSVAL(0);
        if (idp)
            *idp = INT_TO_JSVAL(totalCount);
        return JS_TRUE;

    case JSENUMERATE_NEXT:
    {
        int index = JSVAL_TO_INT(*statep);
        int count = totalCount;

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

JSObject*
nsXPCWrappedNativeClass::NewInstanceJSObject(XPCContext* xpcc,
                                             JSObject* aGlobalObject,
                                             nsXPCWrappedNative* self)
{
    JSContext* cx = xpcc->GetJSContext();
    JSClass* jsclazz = self->GetDynamicScriptable() ?
                            &WrappedNativeWithCall_class :
                            &WrappedNative_class;

    // shaver@mozilla.org sez:
    // We set the prototype to be the global object, then set it back to null
    // after creation.  If we just pass nsnull as the prototype argument, the
    // engine will do a scope search for the class name to find the constructor,
    // which is an expense we don't need, and will always fail anyway.

    JSObject* jsobj = JS_NewObject(cx, jsclazz, aGlobalObject, aGlobalObject);
    if(!jsobj || !JS_SetPrototype(cx, jsobj, nsnull) ||
       !JS_SetPrivate(cx, jsobj, self))
        return nsnull;

    // wrapper is responsible for calling DynamicScriptable->Create
    return jsobj;
}

// static
nsXPCWrappedNative*
nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject(JSContext* cx,
                                                    JSObject* jsobj)
{
    NS_PRECONDITION(jsobj, "bad param");
    JSObject* cur = jsobj;

    while(cur)
    {
        if(JS_InstanceOf(cx, cur, &WrappedNative_class, nsnull) ||
           JS_InstanceOf(cx, cur, &WrappedNativeWithCall_class, nsnull))
            return (nsXPCWrappedNative*) JS_GetPrivate(cx, cur);
        cur = JS_GetPrototype(cx, cur);
    }
    return nsnull;
}


/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedNativeClass::DebugDump(PRInt16 depth)
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
        XPC_LOG_ALWAYS(("mRuntime @ %x", mRuntime));
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
                        AutoPushCompatibleJSContext 
                            autoCX(mRuntime->GetJSRuntime());
                        JSContext* cx = autoCX.GetJSContext();
                        if (cx && JS_IdToValue(cx, desc.id, &idval) &&
                            JSVAL_IS_STRING(idval) &&
                           (name = JS_GetStringBytes(
                                    JSVAL_TO_STRING(idval))) != nsnull)
                        {
                            XPC_LOG_ALWAYS(("property name is %s", name));
                        }
                        XPC_LOG_OUTDENT();
                    }
                    XPC_LOG_ALWAYS(("index is %d", desc.index));
                    XPC_LOG_ALWAYS(("index2 is %d", desc.index2));
                    XPC_LOG_ALWAYS(("argc is %d", desc.argc));
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
