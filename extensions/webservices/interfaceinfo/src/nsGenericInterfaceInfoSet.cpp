/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
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

/* The nsGenericInterfaceInfo/nsGenericInterfaceInfoSet implementations.*/

#include "iixprivate.h"


/***************************************************************************/
// implement nsGenericInterfaceInfoSet

#define ARENA_BLOCK_SIZE (1024 * 1)

NS_IMPL_THREADSAFE_ISUPPORTS3(nsGenericInterfaceInfoSet,
                              nsIInterfaceInfoManager,
                              nsIGenericInterfaceInfoSet,
                              nsISupportsWeakReference)

nsGenericInterfaceInfoSet::nsGenericInterfaceInfoSet()
{
    mArena = XPT_NewArena(ARENA_BLOCK_SIZE, sizeof(double),
                          "nsGenericInterfaceInfoSet Arena");
}

nsGenericInterfaceInfoSet::~nsGenericInterfaceInfoSet()
{
    PRInt32 count = mInterfaces.Count();

    for(PRInt32 i = 0; i < count; i++)
    {
        nsIInterfaceInfo* info = (nsIInterfaceInfo*) mInterfaces.ElementAt(i);
        if(CheckOwnedFlag(info))
            delete (nsGenericInterfaceInfo*) ClearOwnedFlag(info);
        else
            NS_RELEASE(info);
    }

    if(mArena)
        XPT_DestroyArena(mArena);
}

nsresult
nsGenericInterfaceInfoSet::IndexOfIID(const nsIID & aIID, PRUint16 *_retval)
{
    PRInt32 count = mInterfaces.Count();

    for(PRInt32 i = 0; i < count; i++)
    {
        nsIInterfaceInfo* info = (nsIInterfaceInfo*)
            ClearOwnedFlag(mInterfaces.ElementAt(i));
        const nsID* iid;
        nsresult rv = info->GetIIDShared(&iid);
        if(NS_FAILED(rv))
            return rv;
        if(iid->Equals(aIID))
        {
            *_retval = (PRUint16) i;
            return NS_OK;
        }
    }
    return NS_ERROR_NO_INTERFACE;
}

nsresult
nsGenericInterfaceInfoSet::IndexOfName(const char* aName, PRUint16 *_retval)
{
    PRInt32 count = mInterfaces.Count();

    for(PRInt32 i = 0; i < count; i++)
    {
        nsIInterfaceInfo* info = (nsIInterfaceInfo*)
            ClearOwnedFlag(mInterfaces.ElementAt(i));
        const char* name;
        nsresult rv = info->GetNameShared(&name);
        if(NS_FAILED(rv))
            return rv;
        if(!strcmp(name, aName))
        {
            *_retval = (PRUint16) i;
            return NS_OK;
        }
    }
    return NS_ERROR_NO_INTERFACE;
}

/************************************************/
// nsIGenericInterfaceInfoSet methods...

/* XPTParamDescriptorPtr allocateParamArray (in PRUint16 aCount); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::AllocateParamArray(PRUint16 aCount,
                                              XPTParamDescriptor * *_retval)
{
    *_retval = (XPTParamDescriptor*)
        XPT_MALLOC(GetArena(), sizeof(XPTParamDescriptor) * aCount);
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* XPTTypeDescriptorPtr allocateAdditionalType (out PRUint16 aIndex); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::AllocateAdditionalType(PRUint16 *aIndex,
                                                  XPTTypeDescriptor * *_retval)
{
    *_retval = (XPTTypeDescriptor*)
        XPT_MALLOC(GetArena(), sizeof(XPTTypeDescriptor));
    if(!*_retval || !mAdditionalTypes.AppendElement(*_retval))
        return NS_ERROR_OUT_OF_MEMORY;
    *aIndex = (PRUint16) mAdditionalTypes.Count()-1;
    return NS_OK;
}

/* PRUint16 createAndAppendInterface (in string aName, in nsIIDRef
   aIID, in PRUint16 aParent, in PRUint8 aFlags, out
   nsIGenericInterfaceInfo aInfo); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::CreateAndAppendInterface(const char *aName,
                                                    const nsIID & aIID,
                                                    PRUint16 aParent,
                                                    PRUint8 aFlags,
                                                    nsIGenericInterfaceInfo **aInfo,
                                                    PRUint16 *_retval)
{
    nsGenericInterfaceInfo* info =
        new nsGenericInterfaceInfo(this, aName, aIID,
                                   (aParent == (PRUint16) -1) ?
                                        nsnull : InfoAtNoAddRef(aParent),
                                   aFlags);
    if(!info || !mInterfaces.AppendElement(SetOwnedFlag(info)))
        return NS_ERROR_OUT_OF_MEMORY;

    *_retval = (PRUint16) mInterfaces.Count()-1;
    return CallQueryInterface(info, aInfo);
}

/* PRUint16 appendExternalInterface (in nsIInterfaceInfo aInfo); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::AppendExternalInterface(nsIInterfaceInfo *aInfo,
                                                   PRUint16 *_retval)
{
    if(!mInterfaces.AppendElement(aInfo))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(aInfo);
    *_retval = (PRUint16) mInterfaces.Count()-1;
    return NS_OK;
}

/* PRUint16 indexOf (in nsIIDRef aIID); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::IndexOf(const nsIID & aIID, PRUint16 *_retval)
{
    return IndexOfIID(aIID, _retval);
}

/* PRUint16 indexOfByName (in string aName); */
NS_IMETHODIMP nsGenericInterfaceInfoSet::IndexOfByName(const char *aName,
                                                       PRUint16 *_retval)
{
    return IndexOfName(aName, _retval);
}

/* nsIInterfaceInfo interfaceInfoAt (in PRUint16 aIndex); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::InterfaceInfoAt(PRUint16 aIndex,
                                           nsIInterfaceInfo **_retval)
{
    NS_ASSERTION(aIndex < (PRUint16)mInterfaces.Count(), "bad index");

    *_retval = (nsIInterfaceInfo*)
        ClearOwnedFlag(mInterfaces.ElementAt(aIndex));
    NS_ADDREF(*_retval);
    return NS_OK;
}

/************************************************/
// nsIInterfaceInfoManager methods...

/* nsIInterfaceInfo getInfoForIID (in nsIIDPtr iid); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::GetInfoForIID(const nsIID * iid,
                                         nsIInterfaceInfo **_retval)
{
    PRUint16 index;
    nsresult rv = IndexOfIID(*iid, &index);
    if(NS_FAILED(rv))
        return rv;
    return InterfaceInfoAt(index, _retval);
}

/* nsIInterfaceInfo getInfoForName (in string name); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::GetInfoForName(const char *name,
                                          nsIInterfaceInfo **_retval)
{
    PRUint16 index;
    nsresult rv = IndexOfName(name, &index);
    if(NS_FAILED(rv))
        return rv;
    return InterfaceInfoAt(index, _retval);
}

/* nsIIDPtr getIIDForName (in string name); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::GetIIDForName(const char *name, nsIID **_retval)
{
    PRUint16 index;
    nsresult rv = IndexOfName(name, &index);
    if(NS_FAILED(rv))
        return rv;

    nsIInterfaceInfo* info = InfoAtNoAddRef(index);
    if(!info)
        return NS_ERROR_FAILURE;

    return info->GetInterfaceIID(_retval);
}

/* string getNameForIID (in nsIIDPtr iid); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::GetNameForIID(const nsIID * iid, char **_retval)
{
    PRUint16 index;
    nsresult rv = IndexOfIID(*iid, &index);
    if(NS_FAILED(rv))
        return rv;

    nsIInterfaceInfo* info = InfoAtNoAddRef(index);
    if(!info)
        return NS_ERROR_FAILURE;

    return info->GetName(_retval);
}

/* nsIEnumerator enumerateInterfaces (); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::EnumerateInterfaces(nsIEnumerator **_retval)
{
    return EnumerateInterfacesWhoseNamesStartWith(nsnull, _retval);
}

/* void autoRegisterInterfaces (); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::AutoRegisterInterfaces()
{
    // NOP
    return NS_OK;
}

/* nsIEnumerator enumerateInterfacesWhoseNamesStartWith (in string prefix); */
NS_IMETHODIMP
nsGenericInterfaceInfoSet::EnumerateInterfacesWhoseNamesStartWith(const char *prefix,
                                                                  nsIEnumerator **_retval)
{
    int count = (int) mInterfaces.Count();
    int len = prefix ? PL_strlen(prefix) : 0;
    const char* name;

    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    for(PRInt32 i = 0; i < count; i++)
    {
        nsIInterfaceInfo* info = InfoAtNoAddRef(i);
        if(!info)
            continue;
        if(!prefix ||
           (NS_SUCCEEDED(info->GetNameShared(&name)) &&
            name == PL_strnstr(name, prefix, len)))
        {
            if(!array->AppendElement(info))
                return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    return array->Enumerate(_retval);
}

/***************************************************************************/
/***************************************************************************/
// implement nsGenericInterfaceInfo

NS_IMPL_QUERY_INTERFACE2(nsGenericInterfaceInfo,
                         nsIInterfaceInfo,
                         nsIGenericInterfaceInfo)

NS_IMETHODIMP_(nsrefcnt)
nsGenericInterfaceInfo::AddRef()
{
    return mSet->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsGenericInterfaceInfo::Release()
{
    return mSet->Release();
}

nsGenericInterfaceInfo::nsGenericInterfaceInfo(nsGenericInterfaceInfoSet* aSet,
                                               const char *aName,
                                               const nsIID & aIID,
                                               nsIInterfaceInfo* aParent,
                                               PRUint8 aFlags)
    : mName(nsnull),
      mIID(aIID),
      mSet(aSet),
      mParent(aParent),
      mFlags(aFlags)
{
    if(mParent)
    {
        mParent->GetMethodCount(&mMethodBaseIndex);
        mParent->GetConstantCount(&mConstantBaseIndex);
    }
    else
    {
        mMethodBaseIndex = mConstantBaseIndex = 0;
    }

    int len = PL_strlen(aName);
    mName = (char*) XPT_MALLOC(mSet->GetArena(), len+1);
    if(mName)
        memcpy(mName, aName, len);
}

/************************************************/
// nsIGenericInterfaceInfo methods...

/* PRUint16 appendMethod (in XPTMethodDescriptorPtr aMethod); */
NS_IMETHODIMP
nsGenericInterfaceInfo::AppendMethod(XPTMethodDescriptor * aMethod,
                                     PRUint16 *_retval)
{
    XPTMethodDescriptor* desc = (XPTMethodDescriptor*)
        XPT_MALLOC(mSet->GetArena(), sizeof(XPTMethodDescriptor));
    if(!desc)
        return NS_ERROR_OUT_OF_MEMORY;

    memcpy(desc, aMethod, sizeof(XPTMethodDescriptor));

    int len = PL_strlen(aMethod->name);
    desc->name = (char*) XPT_MALLOC(mSet->GetArena(), len+1);
    if(!desc->name)
        return NS_ERROR_OUT_OF_MEMORY;

    // XPT_MALLOC returns zeroed out memory, no need to copy the
    // terminating null character.
    memcpy(desc->name, aMethod->name, len);

    return mMethods.AppendElement(desc) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* PRUint16 appendConst (in XPTConstDescriptorPtr aConst); */
NS_IMETHODIMP
nsGenericInterfaceInfo::AppendConst(XPTConstDescriptor * aConst,
                                    PRUint16 *_retval)
{
    NS_ASSERTION(aConst->type.prefix.flags == TD_INT16  ||
                 aConst->type.prefix.flags == TD_UINT16 ||
                 aConst->type.prefix.flags == TD_INT32  ||
                 aConst->type.prefix.flags == TD_UINT32,
                 "unsupported const type");

    XPTConstDescriptor* desc = (XPTConstDescriptor*)
        XPT_MALLOC(mSet->GetArena(), sizeof(XPTConstDescriptor));
    if(!desc)
        return NS_ERROR_OUT_OF_MEMORY;

    memcpy(desc, aConst, sizeof(XPTConstDescriptor));

    int len = PL_strlen(aConst->name);
    desc->name = (char*) XPT_MALLOC(mSet->GetArena(), len+1);
    if(!desc->name)
        return NS_ERROR_OUT_OF_MEMORY;

    // XPT_MALLOC returns zeroed out memory, no need to copy the
    // terminating null character.
    memcpy(desc->name, aConst->name, len);

    return mConstants.AppendElement(desc) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/************************************************/
// nsIInterfaceInfo methods...

/* readonly attribute string name; */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetName(char * *aName)
{
    *aName = (char*) nsMemory::Clone(mName, PL_strlen(mName)+1);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsIIDPtr InterfaceIID; */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetInterfaceIID(nsIID * *aIID)
{
    *aIID = (nsIID*) nsMemory::Clone(&mIID, sizeof(nsIID));
    return *aIID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* PRBool isScriptable (); */
NS_IMETHODIMP
nsGenericInterfaceInfo::IsScriptable(PRBool *_retval)
{
    *_retval = XPT_ID_IS_SCRIPTABLE(mFlags) != 0;
    return NS_OK;
}

/* readonly attribute nsIInterfaceInfo parent; */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetParent(nsIInterfaceInfo * *aParent)
{
    *aParent = mParent;
    NS_IF_ADDREF(*aParent);
    return NS_OK;
}

/* readonly attribute PRUint16 methodCount; */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetMethodCount(PRUint16 *aMethodCount)
{
    *aMethodCount = mMethodBaseIndex + (PRUint16) mMethods.Count();
    return NS_OK;
}

/* readonly attribute PRUint16 constantCount; */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetConstantCount(PRUint16 *aConstantCount)
{
    *aConstantCount = mConstantBaseIndex + (PRUint16) mConstants.Count();
    return NS_OK;
}

/* void getMethodInfo (in PRUint16 index, [shared, retval] out
   nsXPTMethodInfoPtr info); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetMethodInfo(PRUint16 index,
                                      const nsXPTMethodInfo * *info)
{
    if(index < mMethodBaseIndex)
        return mParent->GetMethodInfo(index, info);
    *info = (const nsXPTMethodInfo *) mMethods.ElementAt(index-mMethodBaseIndex);
    return NS_OK;
}

/* void getMethodInfoForName (in string methodName, out PRUint16
   index, [shared, retval] out nsXPTMethodInfoPtr info); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetMethodInfoForName(const char *methodName,
                                             PRUint16 *index,
                                             const nsXPTMethodInfo * *info)
{
    PRUint16 count = mMethodBaseIndex + (PRUint16) mMethods.Count();
    for(PRUint16 i = 0; i < count; i++)
    {
        const nsXPTMethodInfo* current;
        nsresult rv = GetMethodInfo(i, &current);
        if(NS_FAILED(rv))
            return rv;

        if(!PL_strcmp(methodName, current->GetName()))
        {
            *index = i;
            *info = current;
            return NS_OK;
        }
    }
    *index = 0;
    *info = 0;
    return NS_ERROR_INVALID_ARG;
}

/* void getConstant (in PRUint16 index, [shared, retval] out
   nsXPTConstantPtr constant); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetConstant(PRUint16 index,
                                    const nsXPTConstant * *constant)
{
    if(index < mConstantBaseIndex)
        return mParent->GetConstant(index, constant);
    *constant = (const nsXPTConstant *)
        mConstants.ElementAt(index-mConstantBaseIndex);
    return NS_OK;
}

/* nsIInterfaceInfo getInfoForParam (in PRUint16 methodIndex, [const]
   in nsXPTParamInfoPtr param); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetInfoForParam(PRUint16 methodIndex,
                                        const nsXPTParamInfo * param,
                                        nsIInterfaceInfo **_retval)
{
    if(methodIndex < mMethodBaseIndex)
        return mParent->GetInfoForParam(methodIndex, param, _retval);

    const XPTTypeDescriptor* td = GetPossiblyNestedType(param);
    NS_ASSERTION(XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE,
                 "not an interface");

    return mSet->InterfaceInfoAt(td->type.iface, _retval);
}

/* nsIIDPtr getIIDForParam (in PRUint16 methodIndex, [const] in
   nsXPTParamInfoPtr param); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetIIDForParam(PRUint16 methodIndex,
                                       const nsXPTParamInfo * param,
                                       nsIID * *_retval)
{
    if(methodIndex < mMethodBaseIndex)
        return mParent->GetIIDForParam(methodIndex, param, _retval);

    const XPTTypeDescriptor* td = GetPossiblyNestedType(param);
    NS_ASSERTION(XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE,
                 "not an interface");

    nsIInterfaceInfo* info = mSet->InfoAtNoAddRef(td->type.iface);
    if(!info)
        return NS_ERROR_FAILURE;

    return info->GetInterfaceIID(_retval);
}

/* nsXPTType getTypeForParam (in PRUint16 methodIndex, [const] in
   nsXPTParamInfoPtr param, in PRUint16 dimension); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetTypeForParam(PRUint16 methodIndex,
                                        const nsXPTParamInfo * param,
                                        PRUint16 dimension, nsXPTType *_retval)
{
    if(methodIndex < mMethodBaseIndex)
        return mParent->GetTypeForParam(methodIndex, param, dimension,
                                        _retval);

    const XPTTypeDescriptor *td =
        dimension ? GetTypeInArray(param, dimension) : &param->type;

    *_retval = nsXPTType(td->prefix);
    return NS_OK;
}

/* PRUint8 getSizeIsArgNumberForParam (in PRUint16 methodIndex,
   [const] in nsXPTParamInfoPtr param, in PRUint16 dimension); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetSizeIsArgNumberForParam(PRUint16 methodIndex,
                                                   const nsXPTParamInfo *param,
                                                   PRUint16 dimension,
                                                   PRUint8 *_retval)
{
    if(methodIndex < mMethodBaseIndex)
        return mParent->GetSizeIsArgNumberForParam(methodIndex, param,
                                                   dimension, _retval);

    const XPTTypeDescriptor *td =
        dimension ? GetTypeInArray(param, dimension) : &param->type;

    *_retval = td->argnum;
    return NS_OK;
}

/* PRUint8 getLengthIsArgNumberForParam (in PRUint16 methodIndex,
   [const] in nsXPTParamInfoPtr param, in PRUint16 dimension); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetLengthIsArgNumberForParam(PRUint16 methodIndex,
                                                     const nsXPTParamInfo *param,
                                                     PRUint16 dimension,
                                                     PRUint8 *_retval)
{
    if(methodIndex < mMethodBaseIndex)
        return mParent->GetLengthIsArgNumberForParam(methodIndex, param,
                                                     dimension, _retval);

    const XPTTypeDescriptor *td =
        dimension ? GetTypeInArray(param, dimension) : &param->type;

    *_retval = td->argnum2;
    return NS_OK;
}

/* PRUint8 getInterfaceIsArgNumberForParam (in PRUint16 methodIndex,
   [const] in nsXPTParamInfoPtr param); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetInterfaceIsArgNumberForParam(PRUint16 methodIndex,
                                                        const nsXPTParamInfo *param,
                                                        PRUint8 *_retval)
{
    if(methodIndex < mMethodBaseIndex)
        return mParent->GetInterfaceIsArgNumberForParam(methodIndex, param,
                                                        _retval);

    const XPTTypeDescriptor* td = GetPossiblyNestedType(param);
    NS_ASSERTION(XPT_TDP_TAG(td->prefix) == TD_INTERFACE_IS_TYPE,
                 "not an interface");

    *_retval = td->argnum;
    return NS_OK;
}

/* PRBool isIID (in nsIIDPtr IID); */
NS_IMETHODIMP
nsGenericInterfaceInfo::IsIID(const nsIID * IID, PRBool *_retval)
{
    *_retval = mIID.Equals(*IID);
    return NS_OK;
}

/* void getNameShared ([shared, retval] out string name); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetNameShared(const char **name)
{
    *name = mName;
    return NS_OK;
}

/* void getIIDShared ([shared, retval] out nsIIDPtrShared iid); */
NS_IMETHODIMP
nsGenericInterfaceInfo::GetIIDShared(const nsIID * *iid)
{
    *iid = &mIID;
    return NS_OK;
}

/* PRBool isFunction (); */
NS_IMETHODIMP
nsGenericInterfaceInfo::IsFunction(PRBool *_retval)
{
    *_retval = XPT_ID_IS_FUNCTION(mFlags) != 0;
    return NS_OK;
}

/* PRBool hasAncestor (in nsIIDPtr iid); */
NS_IMETHODIMP
nsGenericInterfaceInfo::HasAncestor(const nsIID * iid, PRBool *_retval)
{
    *_retval = PR_FALSE;

    nsCOMPtr<nsIInterfaceInfo> current =
        NS_STATIC_CAST(nsIInterfaceInfo*, this);
    while(current)
    {
        PRBool same;
        if(NS_SUCCEEDED(current->IsIID(iid, &same)) && same)
        {
            *_retval = PR_TRUE;
            break;
        }
        nsCOMPtr<nsIInterfaceInfo> temp(current);
        temp->GetParent(getter_AddRefs(current));
    }
    return NS_OK;
}

/* [notxpcom] nsresult getIIDForParamNoAlloc (in PRUint16 methodIndex,
   [const] in nsXPTParamInfoPtr param, out nsIID iid); */
NS_IMETHODIMP_(nsresult)
nsGenericInterfaceInfo::GetIIDForParamNoAlloc(PRUint16 methodIndex,
                                              const nsXPTParamInfo * param,
                                              nsIID *iid)
{
    if(methodIndex < mMethodBaseIndex)
        return mParent->GetIIDForParamNoAlloc(methodIndex, param, iid);

    const XPTTypeDescriptor* td = GetPossiblyNestedType(param);
    NS_ASSERTION(XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE,
                 "not an interface");

    nsIInterfaceInfo* info = mSet->InfoAtNoAddRef(td->type.iface);
    if(!info)
        return NS_ERROR_FAILURE;

    const nsIID* iidp;
    nsresult rv = info->GetIIDShared(&iidp);
    if(NS_FAILED(rv))
        return rv;

    *iid = *iidp;
    return NS_OK;
}
