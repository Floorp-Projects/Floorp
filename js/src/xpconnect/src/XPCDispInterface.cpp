/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the IDispatch implementation for XPConnect
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/** \file XPCDispInterface.cpp
 * XPCDispInterface implementation
 * This file contains the implementation of the XPCDispInterface class
 */

#include "xpcprivate.h"

/**
 * Is this function reflectable
 * This function determines if we should reflect a particular method of an
 * interface
 */
inline
PRBool IsReflectable(FUNCDESC * pFuncDesc)
{
    return (pFuncDesc->wFuncFlags&FUNCFLAG_FRESTRICTED) == 0 &&
           pFuncDesc->funckind == FUNC_DISPATCH;
}

/**
 * Counts the reflectable methods
 */
PRUint32 GetReflectableCount(ITypeInfo * pTypeInfo, PRUint32 members)
{
    DISPID lastDispID = 0;
    PRUint32 result = 0;
    for(UINT iMethod = 0; iMethod < members; iMethod++ )
    {
        FUNCDESC* pFuncDesc;
        if(SUCCEEDED(pTypeInfo->GetFuncDesc(iMethod, &pFuncDesc)))
        {
            // Only add the function to our list if it is at least at nesting level
            // 2 (i.e. defined in an interface derived from IDispatch).
            if(lastDispID != pFuncDesc->memid && IsReflectable(pFuncDesc))
            {
                ++result;
                lastDispID = pFuncDesc->memid;
            }
            pTypeInfo->ReleaseFuncDesc(pFuncDesc);
        }
    }
    return result;
}

XPCDispInterface* XPCDispInterface::NewInstance(JSContext* cx, nsISupports * pIface)
{
    IDispatch * pDispatch;
    HRESULT hr = NS_REINTERPRET_CAST(IUnknown*,pIface)->QueryInterface(IID_IDispatch, (PVOID*)&pDispatch);

    if(SUCCEEDED(hr) && pDispatch)
    {
        unsigned int count;
        hr = pDispatch->GetTypeInfoCount(&count);
        if(SUCCEEDED(hr) && count > 0)
        {
            ITypeInfo* pPtr;
            hr = pDispatch->GetTypeInfo(0,LOCALE_SYSTEM_DEFAULT, &pPtr);
            CComPtr<ITypeInfo> pTypeInfo;
            pTypeInfo.Attach(pPtr);
            if(SUCCEEDED(hr))
            {
                TYPEATTR * attr;
                hr = pTypeInfo->GetTypeAttr(&attr);
                if(SUCCEEDED(hr))
                {
                    UINT funcs = attr->cFuncs;
                    pTypeInfo->ReleaseTypeAttr(attr);
                    PRUint32 memberCount = GetReflectableCount(pTypeInfo, funcs);
                    return new (memberCount) XPCDispInterface(cx, pTypeInfo, funcs);
                }
            }
        }
    }
    return nsnull;
}

/**
 * Sets a members type based on COM's INVOKEKIND
 */
static
void ConvertInvokeKind(INVOKEKIND invokeKind, XPCDispInterface::Member & member)
{
    switch (invokeKind)
    {
        case INVOKE_FUNC:
        {
            member.SetFunction();
        }
        break;
        case INVOKE_PROPERTYGET:
        {
            member.MakeGetter();
        }
        break;
        case INVOKE_PROPERTYPUT:
        {
            member.MakeSetter();
        }
        break;
        // TODO: Handle putref
        default:
        {
            NS_ERROR("Invalid invoke kind found in COM type info");
        }
        break;
    }
}

void XPCDispInterface::InspectIDispatch(JSContext * cx, ITypeInfo * pTypeInfo, PRUint32 members)
{
    HRESULT hResult;
    DISPID lastDispID = 0;

    XPCDispInterface::Member * pInfo = mMembers;
    mMemberCount = 0;
    for(PRUint32 index = 0; index < members; index++ )
    {
        FUNCDESC* pFuncDesc;
        hResult = pTypeInfo->GetFuncDesc( index, &pFuncDesc );
        if(SUCCEEDED( hResult ))
        {
            PRBool release = PR_TRUE;
            if(IsReflectable(pFuncDesc))
            {
                // Check and see if the previous dispid was the same
                if(lastDispID != pFuncDesc->memid)
                {
                    BSTR name;
                    UINT nameCount;
                    if(SUCCEEDED(pTypeInfo->GetNames(
                        pFuncDesc->memid,
                        &name,
                        1,
                        &nameCount)))
                    {
                        JSString* str = JS_InternUCString(cx, name);
                        ::SysFreeString(name);
                        // Initialize
                        pInfo = new (pInfo) Member;
                        pInfo->SetName(STRING_TO_JSVAL(str));
                        lastDispID = pFuncDesc->memid;
                        pInfo->ResetType();
                        ConvertInvokeKind(pFuncDesc->invkind, *pInfo);
                        pInfo->SetTypeInfo(pFuncDesc->memid, pTypeInfo, pFuncDesc);
                        release = PR_FALSE;
                        ++pInfo;
                        ++mMemberCount;
                    }
                }
                // if it was then we're on the second part of the
                // property
                else
                {
                    ConvertInvokeKind(pFuncDesc->invkind, *(pInfo - 1));
                }
            }
            if(release)
            {
                pTypeInfo->ReleaseFuncDesc(pFuncDesc);
            }
        }
    }
}

JSBool XPCDispInterface::Member::GetValue(XPCCallContext& ccx,
                                          XPCNativeInterface * iface, 
                                          jsval * retval) const
{
    // This is a method or attribute - we'll be needing a function object

    // We need to use the safe context for this thread because we don't want
    // to parent the new (and cached forever!) function object to the current
    // JSContext's global object. That would be bad!
    if((mType & RESOLVED) == 0)
    {
        JSContext* cx = ccx.GetSafeJSContext();
        if(!cx)
            return JS_FALSE;

        intN argc;
        intN flags;
        JSNative callback;
        if(IsFunction())
        {
            argc = GetParamCount();
            flags = 0;
            callback = XPC_IDispatch_CallMethod;
        }
        else
        {
            if(IsSetter())
            {
                flags = JSFUN_GETTER | JSFUN_SETTER;
            }
            else
            {
                flags = JSFUN_GETTER;
            }
            argc = 0;
            callback = XPC_IDispatch_GetterSetter;
        }

        JSFunction *fun = JS_NewFunction(cx, callback, argc, flags, nsnull,
                                         JS_GetStringBytes(JSVAL_TO_STRING(mName)));
        if(!fun)
            return JS_FALSE;

        JSObject* funobj = JS_GetFunctionObject(fun);
        if(!funobj)
            return JS_FALSE;

        // Store ourselves and our native interface within the JSObject
        if(!JS_SetReservedSlot(ccx, funobj, 0, PRIVATE_TO_JSVAL(this)))
            return JS_FALSE;

        if(!JS_SetReservedSlot(ccx, funobj, 1, PRIVATE_TO_JSVAL(iface)))
            return JS_FALSE;

        {   // scoped lock
            XPCAutoLock lock(ccx.GetRuntime()->GetMapLock());
            NS_CONST_CAST(Member*,this)->mVal = OBJECT_TO_JSVAL(funobj);
            NS_CONST_CAST(Member*,this)->mType |= RESOLVED;
        }
    }
    *retval = mVal;
    return JS_TRUE;
}
