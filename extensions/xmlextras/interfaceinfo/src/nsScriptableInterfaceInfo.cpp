/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* nsIScriptableInteraceInfo family implementations. */

#include "iixprivate.h"

/***************************************************************************/
static inline nsresult CloneString(const char* inStr, char** outStr)
{
    *outStr = (char*) nsMemory::Clone(inStr, strlen(inStr)+1);
    return *outStr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/***************************************************************************/

/* Header file */
class nsScriptableDataType : public nsIScriptableDataType
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLEDATATYPE

    static nsresult Create(const nsXPTType& aType,
                           nsIScriptableDataType** aResult);

    nsScriptableDataType(); // not implemented

    nsScriptableDataType(const nsXPTType& aType)
        : mType(aType) {}

    virtual ~nsScriptableDataType() {}

private:
    nsXPTType mType;
};

// static
nsresult
nsScriptableDataType::Create(const nsXPTType& aType,
                             nsIScriptableDataType** aResult)
{
    nsScriptableDataType* obj = new nsScriptableDataType(aType);
    if(!obj)
        return NS_ERROR_OUT_OF_MEMORY;
    *aResult = NS_STATIC_CAST(nsIScriptableDataType*, obj);
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsScriptableDataType, nsIScriptableDataType)

/* readonly attribute PRBool isPointer; */
NS_IMETHODIMP
nsScriptableDataType::GetIsPointer(PRBool *aIsPointer)
{
    *aIsPointer = mType.IsPointer();
    return NS_OK;
}

/* readonly attribute PRBool isUniquePointer; */
NS_IMETHODIMP
nsScriptableDataType::GetIsUniquePointer(PRBool *aIsUniquePointer)
{
    *aIsUniquePointer = mType.IsUniquePointer();
    return NS_OK;
}

/* readonly attribute PRBool isReference; */
NS_IMETHODIMP
nsScriptableDataType::GetIsReference(PRBool *aIsReference)
{
    *aIsReference = mType.IsReference();
    return NS_OK;
}

/* readonly attribute PRBool isArithmetic; */
NS_IMETHODIMP
nsScriptableDataType::GetIsArithmetic(PRBool *aIsArithmetic)
{
    *aIsArithmetic = mType.IsArithmetic();
    return NS_OK;
}

/* readonly attribute PRBool isInterfacePointer; */
NS_IMETHODIMP
nsScriptableDataType::GetIsInterfacePointer(PRBool *aIsInterfacePointer)
{
    *aIsInterfacePointer = mType.IsInterfacePointer();
    return NS_OK;
}

/* readonly attribute PRBool isArray; */
NS_IMETHODIMP
nsScriptableDataType::GetIsArray(PRBool *aIsArray)
{
    *aIsArray = mType.IsArray();
    return NS_OK;
}

/* readonly attribute PRBool isDependent; */
NS_IMETHODIMP
nsScriptableDataType::GetIsDependent(PRBool *aIsDependent)
{
    *aIsDependent = mType.IsDependent();
    return NS_OK;
}

/* readonly attribute PRUint16 dataType; */
NS_IMETHODIMP
nsScriptableDataType::GetDataType(PRUint16 *aDataType)
{
    *aDataType = mType.TagPart();
    return NS_OK;
}

/***************************************************************************/

class nsScriptableParamInfo : public nsIScriptableParamInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLEPARAMINFO

    static nsresult Create(nsIInterfaceInfo* aInfo,
                           const nsXPTParamInfo& aParam,
                           nsIScriptableParamInfo** aResult);

    nsScriptableParamInfo(); // not implemented
    nsScriptableParamInfo(nsIInterfaceInfo* aInfo,
                           const nsXPTParamInfo& aParam)
        : mInfo(aInfo), mParam(aParam) {}
    virtual ~nsScriptableParamInfo() {}

private:
    // Holding onto the interface info keeps the underlying param alive.
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    nsXPTParamInfo             mParam;
};


// static
nsresult
nsScriptableParamInfo::Create(nsIInterfaceInfo* aInfo,
                              const nsXPTParamInfo& aParam,
                              nsIScriptableParamInfo** aResult)
{
    nsScriptableParamInfo* obj = new nsScriptableParamInfo(aInfo, aParam);
    if(!obj)
        return NS_ERROR_OUT_OF_MEMORY;
    *aResult = NS_STATIC_CAST(nsIScriptableParamInfo*, obj);
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsScriptableParamInfo, nsIScriptableParamInfo)

/* readonly attribute PRBool isIn; */
NS_IMETHODIMP
nsScriptableParamInfo::GetIsIn(PRBool *aIsIn)
{
    *aIsIn = mParam.IsIn();
    return NS_OK;
}

/* readonly attribute PRBool isOut; */
NS_IMETHODIMP
nsScriptableParamInfo::GetIsOut(PRBool *aIsOut)
{
    *aIsOut = mParam.IsOut();
    return NS_OK;
}

/* readonly attribute PRBool isRetval; */
NS_IMETHODIMP
nsScriptableParamInfo::GetIsRetval(PRBool *aIsRetval)
{
    *aIsRetval = mParam.IsRetval();
    return NS_OK;
}

/* readonly attribute PRBool isShared; */
NS_IMETHODIMP
nsScriptableParamInfo::GetIsShared(PRBool *aIsShared)
{
    *aIsShared = mParam.IsShared();
    return NS_OK;
}

/* readonly attribute PRBool isDipper; */
NS_IMETHODIMP
nsScriptableParamInfo::GetIsDipper(PRBool *aIsDipper)
{
    *aIsDipper = mParam.IsDipper();
    return NS_OK;
}

/* readonly attribute nsIScriptableDataType type; */
NS_IMETHODIMP
nsScriptableParamInfo::GetType(nsIScriptableDataType * *aType)
{
    return nsScriptableDataType::Create(mParam.GetType(), aType);
}

/* [noscript] void getParamInfo ([shared, const, retval] out nsXPTParamInfoPtr aInfo); */
NS_IMETHODIMP
nsScriptableParamInfo::GetParamInfo(const nsXPTParamInfo * *aInfo)
{
    *aInfo = &mParam;
    return NS_OK;
}

/***************************************************************************/
class nsScriptableConstant : public nsIScriptableConstant
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLECONSTANT

    static nsresult Create(nsIInterfaceInfo* aInfo,
                           const nsXPTConstant& aConst,
                           nsIScriptableConstant** aResult);

    nsScriptableConstant(); // not implemented
    nsScriptableConstant(nsIInterfaceInfo* aInfo,
                         const nsXPTConstant& aConst)
        : mInfo(aInfo), mConst(aConst) {}
    virtual ~nsScriptableConstant() {}
private:
    // Holding onto the interface info keeps the underlying const alive.
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    nsXPTConstant              mConst;
};

// static
nsresult
nsScriptableConstant::Create(nsIInterfaceInfo* aInfo,
                             const nsXPTConstant& aConst,
                             nsIScriptableConstant** aResult)
{
    nsScriptableConstant* obj = new nsScriptableConstant(aInfo, aConst);
    if(!obj)
        return NS_ERROR_OUT_OF_MEMORY;
    *aResult = NS_STATIC_CAST(nsIScriptableConstant*, obj);
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsScriptableConstant, nsIScriptableConstant)

/* readonly attribute string name; */
NS_IMETHODIMP
nsScriptableConstant::GetName(char * *aName)
{
    return CloneString(mConst.GetName(), aName);
}

/* readonly attribute nsIScriptableDataType type; */
NS_IMETHODIMP
nsScriptableConstant::GetType(nsIScriptableDataType * *aType)
{
    return nsScriptableDataType::Create(mConst.GetType(), aType);
}

/* readonly attribute nsIVariant value; */
NS_IMETHODIMP
nsScriptableConstant::GetValue(nsIVariant * *aValue)
{
    nsVariant* variant = new nsVariant();
    if(!variant)
    {
        *aValue = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    *aValue = NS_STATIC_CAST(nsIVariant*, variant);
    NS_ADDREF(*aValue);

    const nsXPTCMiniVariant* varval = mConst.GetValue();
    nsresult rv;

    switch(mConst.GetType())
    {
        case nsXPTType::T_I16:
            rv = variant->SetAsInt16(varval->val.i16);
            break;
        case nsXPTType::T_I32:
            rv = variant->SetAsInt32(varval->val.i32);
            break;
        case nsXPTType::T_U16:
            rv = variant->SetAsUint16(varval->val.u16);
            break;
        case nsXPTType::T_U32:
            rv = variant->SetAsUint32(varval->val.u32);
            break;
        default:
            NS_ERROR("invalid const type");
            rv = NS_ERROR_UNEXPECTED;
            break;
    }

    if(NS_FAILED(rv))
    {
        NS_RELEASE(*aValue);
        return rv;
    }

    return NS_OK;
}

/***************************************************************************/

class nsScriptableMethodInfo : public nsIScriptableMethodInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLEMETHODINFO

    static nsresult Create(nsIInterfaceInfo* aInfo,
                           const nsXPTMethodInfo& aMethod,
                           nsIScriptableMethodInfo** aResult);

    nsScriptableMethodInfo(); // not implemented
    nsScriptableMethodInfo(nsIInterfaceInfo* aInfo,
                            const nsXPTMethodInfo& aMethod)
        : mInfo(aInfo), mMethod(aMethod) {}
    virtual ~nsScriptableMethodInfo() {}
private:
    // Holding onto the interface info keeps the underlying method alive.
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    const nsXPTMethodInfo&     mMethod;
};

// static
nsresult
nsScriptableMethodInfo::Create(nsIInterfaceInfo* aInfo,
                               const nsXPTMethodInfo& aMethod,
                               nsIScriptableMethodInfo** aResult)
{
    nsScriptableMethodInfo* obj = new nsScriptableMethodInfo(aInfo, aMethod);
    if(!obj)
        return NS_ERROR_OUT_OF_MEMORY;
    *aResult = NS_STATIC_CAST(nsIScriptableMethodInfo*, obj);
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsScriptableMethodInfo, nsIScriptableMethodInfo)

/* readonly attribute PRBool isGetter; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetIsGetter(PRBool *aIsGetter)
{
    *aIsGetter = mMethod.IsGetter();
    return NS_OK;
}

/* readonly attribute PRBool isSetter; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetIsSetter(PRBool *aIsSetter)
{
    *aIsSetter = mMethod.IsSetter();
    return NS_OK;
}

/* readonly attribute PRBool isNotXPCOM; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetIsNotXPCOM(PRBool *aIsNotXPCOM)
{
    *aIsNotXPCOM = mMethod.IsNotXPCOM();
    return NS_OK;
}

/* readonly attribute PRBool isConstructor; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetIsConstructor(PRBool *aIsConstructor)
{
    *aIsConstructor = mMethod.IsConstructor();
    return NS_OK;
}

/* readonly attribute PRBool isHidden; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetIsHidden(PRBool *aIsHidden)
{
    *aIsHidden = mMethod.IsHidden();
    return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetName(char * *aName)
{
    return CloneString(mMethod.GetName(), aName);
}

/* readonly attribute PRUint8 paramCount; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetParamCount(PRUint8 *aParamCount)
{
    *aParamCount = mMethod.GetParamCount();
    return NS_OK;
}

/* nsIScriptableParamInfo getParam (in PRUint8 idx); */
NS_IMETHODIMP
nsScriptableMethodInfo::GetParam(PRUint8 idx, nsIScriptableParamInfo **_retval)
{
    if(idx >= mMethod.GetParamCount())
        return NS_ERROR_INVALID_ARG;
    return nsScriptableParamInfo::Create(mInfo, mMethod.GetParam(idx), _retval);
}

/* readonly attribute nsIScriptableParamInfo result; */
NS_IMETHODIMP
nsScriptableMethodInfo::GetResult(nsIScriptableParamInfo * *aResult)
{
    return nsScriptableParamInfo::Create(mInfo, mMethod.GetResult(), aResult);
}

/***************************************************************************/

// static
nsresult
nsScriptableInterfaceInfo::Create(nsIInterfaceInfo* aInfo,
                                  nsIScriptableInterfaceInfo** aResult)
{
    nsScriptableInterfaceInfo* obj = new nsScriptableInterfaceInfo(aInfo);
    if(!obj)
        return NS_ERROR_OUT_OF_MEMORY;
    *aResult = NS_STATIC_CAST(nsIScriptableInterfaceInfo*, obj);
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsScriptableInterfaceInfo, nsIScriptableInterfaceInfo)

nsScriptableInterfaceInfo::nsScriptableInterfaceInfo()
{
}

nsScriptableInterfaceInfo::nsScriptableInterfaceInfo(nsIInterfaceInfo* aInfo)
    : mInfo(aInfo)
{
}

nsScriptableInterfaceInfo::~nsScriptableInterfaceInfo()
{
    // empty;
}

/* [noscript] attribute nsIInterfaceInfo info; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetInfo(nsIInterfaceInfo * *aInfo)
{
    if(mInfo)
        NS_ADDREF(*aInfo = mInfo);
    else
        *aInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsScriptableInterfaceInfo::SetInfo(nsIInterfaceInfo * aInfo)
{
    if(mInfo)
        return NS_ERROR_ALREADY_INITIALIZED;
    mInfo = aInfo;
    return NS_OK;
}

/***************************************************************************/

typedef PRBool (*InfoTester)(nsIInterfaceInfoManager* manager, const void* data,
                             nsIInterfaceInfo** info);

static PRBool IIDTester(nsIInterfaceInfoManager* manager, const void* data,
                        nsIInterfaceInfo** info)
{
    return NS_SUCCEEDED(manager->GetInfoForIID((const nsIID *) data, info)) &&
           *info;
}

static PRBool NameTester(nsIInterfaceInfoManager* manager, const void* data,
                      nsIInterfaceInfo** info)
{
    return NS_SUCCEEDED(manager->GetInfoForName((const char *) data, info)) &&
           *info;
}

static nsresult FindInfo(InfoTester tester, const void* data, nsIInterfaceInfo** info)
{
    nsCOMPtr<nsIInterfaceInfoManager> iim = 
        dont_AddRef(XPTI_GetInterfaceInfoManager());
    
    if(!iim)
        return NS_ERROR_UNEXPECTED;

    if(tester(iim, data, info))
        return NS_OK;
    
    // If not found, then let's ask additional managers.

    PRBool yes;
    nsCOMPtr<nsISimpleEnumerator> list;
    nsCOMPtr<nsIInterfaceInfoSuperManager> iism;

    if((nsnull != (iism = do_QueryInterface(iim))) &&
       NS_SUCCEEDED(iism->HasAdditionalManagers(&yes)) && yes &&
       NS_SUCCEEDED(iism->EnumerateAdditionalManagers(getter_AddRefs(list))) &&
       list)
    {
        PRBool more;
        nsCOMPtr<nsIInterfaceInfoManager> current;

        while(NS_SUCCEEDED(list->HasMoreElements(&more)) && more &&
              NS_SUCCEEDED(list->GetNext(getter_AddRefs(current))) && current)
        {
            if(tester(current, data, info))
                return NS_OK;
        }
    }
    
    return NS_ERROR_NO_INTERFACE;
}    

/***************************************************************************/


/* void Init (in nsIIDPtr aIID); */
NS_IMETHODIMP 
nsScriptableInterfaceInfo::Init(const nsIID * aIID)
{
    if(mInfo)
        return NS_ERROR_ALREADY_INITIALIZED;

    if(!aIID)
        return NS_ERROR_NULL_POINTER;

    return FindInfo(IIDTester, aIID, getter_AddRefs(mInfo));
}

/* void initWithName (in string name); */
NS_IMETHODIMP 
nsScriptableInterfaceInfo::InitWithName(const char *name)
{
    if(mInfo)
        return NS_ERROR_ALREADY_INITIALIZED;

    if(!name)
        return NS_ERROR_NULL_POINTER;

    return FindInfo(NameTester, name, getter_AddRefs(mInfo));
}

/* readonly attribute string name; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetName(char * *aName)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->GetName(aName);
}

/* readonly attribute nsIIDPtr interfaceID; */
NS_IMETHODIMP 
nsScriptableInterfaceInfo::GetInterfaceID(nsIID * *aInterfaceID)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->GetInterfaceIID(aInterfaceID);
}

/* readonly attribute PRBool isValid; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetIsValid(PRBool *aIsValid)
{
    *aIsValid = !!mInfo;
    return NS_OK;
}

/* readonly attribute PRBool isScriptable; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetIsScriptable(PRBool *aIsScriptable)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->IsScriptable(aIsScriptable);
}

/* readonly attribute nsIScriptableInterfaceInfo parent; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetParent(nsIScriptableInterfaceInfo * *aParent)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIInterfaceInfo> parentInfo;
    nsresult rv = mInfo->GetParent(getter_AddRefs(parentInfo));
    if(NS_FAILED(rv))
        return rv;

    if(parentInfo)
        return Create(parentInfo, aParent);

    *aParent = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint16 methodCount; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetMethodCount(PRUint16 *aMethodCount)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->GetMethodCount(aMethodCount);
}

/* readonly attribute PRUint16 constantCount; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetConstantCount(PRUint16 *aConstantCount)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->GetConstantCount(aConstantCount);
}

/* nsIScriptableMethodInfo getMethodInfo (in PRUint16 index); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetMethodInfo(PRUint16 index, nsIScriptableMethodInfo **_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTMethodInfo* methodInfo;
    nsresult rv = mInfo->GetMethodInfo(index, &methodInfo);
    if(NS_FAILED(rv))
        return rv;

    return nsScriptableMethodInfo::Create(mInfo, *methodInfo, _retval);
}

/* nsIScriptableMethodInfo getMethodInfoForName (in string methodName, out PRUint16 index); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetMethodInfoForName(const char *methodName, PRUint16 *index, nsIScriptableMethodInfo **_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTMethodInfo* methodInfo;
    nsresult rv = mInfo->GetMethodInfoForName(methodName, index, &methodInfo);
    if(NS_FAILED(rv))
        return rv;

    return nsScriptableMethodInfo::Create(mInfo, *methodInfo, _retval);
}

/* nsIScriptableConstant getConstant (in PRUint16 index); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetConstant(PRUint16 index, nsIScriptableConstant **_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTConstant* constant;
    nsresult rv = mInfo->GetConstant(index, &constant);
    if(NS_FAILED(rv))
        return rv;

    return nsScriptableConstant::Create(mInfo, *constant, _retval);
}

/* nsIScriptableInterfaceInfo getInfoForParam (in PRUint16 methodIndex, in nsIScriptableParamInfo param); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetInfoForParam(PRUint16 methodIndex, nsIScriptableParamInfo *param, nsIScriptableInterfaceInfo **_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTParamInfo* paramInfo;
    nsresult rv = param->GetParamInfo(&paramInfo);
    if(NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIInterfaceInfo> info;
    rv = mInfo->GetInfoForParam(methodIndex, paramInfo, getter_AddRefs(info));
    if(NS_FAILED(rv))
        return rv;

    if(info)
        return Create(info, _retval);

    *_retval = nsnull;
    return NS_OK;
}

/* nsIIDPtr getIIDForParam (in PRUint16 methodIndex, in nsIScriptableParamInfo param); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetIIDForParam(PRUint16 methodIndex, nsIScriptableParamInfo *param, nsIID * *_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTParamInfo* paramInfo;
    nsresult rv = param->GetParamInfo(&paramInfo);
    if(NS_FAILED(rv))
        return rv;

    return mInfo->GetIIDForParam(methodIndex, paramInfo, _retval);
}

/* nsIScriptableDataType getTypeForParam (in PRUint16 methodIndex, in nsIScriptableParamInfo param, in PRUint16 dimension); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetTypeForParam(PRUint16 methodIndex, nsIScriptableParamInfo *param, PRUint16 dimension, nsIScriptableDataType **_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTParamInfo* paramInfo;
    nsresult rv = param->GetParamInfo(&paramInfo);
    if(NS_FAILED(rv))
        return rv;

    nsXPTType type;

    rv = mInfo->GetTypeForParam(methodIndex, paramInfo, dimension, &type);
    if(NS_FAILED(rv))
        return rv;

    return nsScriptableDataType::Create(type, _retval);
}

/* PRUint8 getSizeIsArgNumberForParam (in PRUint16 methodIndex, in nsIScriptableParamInfo param, in PRUint16 dimension); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetSizeIsArgNumberForParam(PRUint16 methodIndex, nsIScriptableParamInfo *param, PRUint16 dimension, PRUint8 *_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTParamInfo* paramInfo;
    nsresult rv = param->GetParamInfo(&paramInfo);
    if(NS_FAILED(rv))
        return rv;

    return mInfo->GetSizeIsArgNumberForParam(methodIndex, paramInfo,
                                             dimension, _retval);
}

/* PRUint8 getLengthIsArgNumberForParam (in PRUint16 methodIndex, in nsIScriptableParamInfo param, in PRUint16 dimension); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetLengthIsArgNumberForParam(PRUint16 methodIndex, nsIScriptableParamInfo *param, PRUint16 dimension, PRUint8 *_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTParamInfo* paramInfo;
    nsresult rv = param->GetParamInfo(&paramInfo);
    if(NS_FAILED(rv))
        return rv;

    return mInfo->GetLengthIsArgNumberForParam(methodIndex, paramInfo,
                                               dimension, _retval);
}

/* PRUint8 getInterfaceIsArgNumberForParam (in PRUint16 methodIndex, in nsIScriptableParamInfo param); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetInterfaceIsArgNumberForParam(PRUint16 methodIndex, nsIScriptableParamInfo *param, PRUint8 *_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    const nsXPTParamInfo* paramInfo;
    nsresult rv = param->GetParamInfo(&paramInfo);
    if(NS_FAILED(rv))
        return rv;

    return mInfo->GetInterfaceIsArgNumberForParam(methodIndex, paramInfo,
                                                  _retval);
}

/* PRBool isIID (in nsIIDPtr IID); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::IsIID(const nsIID * IID, PRBool *_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->IsIID(IID, _retval);
}

/* readonly attribute PRBool isFunction; */
NS_IMETHODIMP
nsScriptableInterfaceInfo::GetIsFunction(PRBool *aIsFunction)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->IsFunction(aIsFunction);
}

/* PRBool hasAncestor (in nsIIDPtr iid); */
NS_IMETHODIMP
nsScriptableInterfaceInfo::HasAncestor(const nsIID * iid, PRBool *_retval)
{
    if(!mInfo)
        return NS_ERROR_NOT_INITIALIZED;

    return mInfo->HasAncestor(iid, _retval);
}
