/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#include "nsWSDLPrivate.h"

// XPCOM includes
#include "nsIServiceManager.h"

// Typelib includes
#include "xptinfo.h"

////////////////////////////////////////////////////////////
//
// nsWSDLInterfaceEnumerator implementation
//
////////////////////////////////////////////////////////////
class nsWSDLInterfaceEnumerator : public nsIEnumerator {
public:
  nsWSDLInterfaceEnumerator(nsWSDLInterfaceSet* aSet);
  virtual ~nsWSDLInterfaceEnumerator();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIENUMERATOR

private:
  nsWeakPtr mSet;
  PRUint16 mCursor;
};

nsWSDLInterfaceEnumerator::nsWSDLInterfaceEnumerator(nsWSDLInterfaceSet* aSet)
  : mCursor(0)
{
  mSet = do_GetWeakReference((nsISupports*)(nsIInterfaceInfoManager*)aSet);
}

nsWSDLInterfaceEnumerator::~nsWSDLInterfaceEnumerator()
{
}

NS_INTERFACE_MAP_BEGIN(nsWSDLInterfaceEnumerator)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIEnumerator)
NS_INTERFACE_MAP_END

/* void first (); */
NS_IMETHODIMP 
nsWSDLInterfaceEnumerator::First()
{
  nsCOMPtr<nsIInterfaceInfoManager> manager(do_QueryReferent(mSet));
  nsWSDLInterfaceSet* set = NS_REINTERPRET_CAST(nsWSDLInterfaceSet*, 
                                                manager.get());
  if (!set) {
    return NS_ERROR_FAILURE;
  }

  mCursor = 0;
  PRUint16 cnt;
  cnt = set->Count();
  if (mCursor < cnt)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

/* void next (); */
NS_IMETHODIMP 
nsWSDLInterfaceEnumerator::Next()
{
  nsCOMPtr<nsIInterfaceInfoManager> manager(do_QueryReferent(mSet));
  nsWSDLInterfaceSet* set = NS_REINTERPRET_CAST(nsWSDLInterfaceSet*, 
                                                manager.get());
  if (!set) {
    return NS_ERROR_FAILURE;
  }

  PRUint16 cnt;
  cnt = set->Count();
  if (mCursor < cnt)   // don't count upward forever
    mCursor++;
  if (mCursor < cnt)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

/* nsISupports currentItem (); */
NS_IMETHODIMP 
nsWSDLInterfaceEnumerator::CurrentItem(nsISupports **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsCOMPtr<nsIInterfaceInfoManager> manager(do_QueryReferent(mSet));
  nsWSDLInterfaceSet* set = NS_REINTERPRET_CAST(nsWSDLInterfaceSet*, 
                                                manager.get());
  if (!set) {
    return NS_ERROR_FAILURE;
  }

  PRUint16 cnt;
  cnt = set->Count();
  if (mCursor >= 0 && mCursor < cnt) {
    nsCOMPtr<nsIInterfaceInfo> info;
    nsresult rv = set->GetInterfaceAt(mCursor + 
                                      NS_WSDLINTERFACEINFOID_RESERVED_BOUNDARY,
                                      getter_AddRefs(info));
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    *_retval = info;
    NS_ADDREF(*_retval);
    
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/* void isDone (); */
NS_IMETHODIMP 
nsWSDLInterfaceEnumerator::IsDone()
{
  nsCOMPtr<nsIInterfaceInfoManager> manager(do_QueryReferent(mSet));
  nsWSDLInterfaceSet* set = NS_REINTERPRET_CAST(nsWSDLInterfaceSet*, 
                                                manager.get());
  if (!set) {
    return NS_ERROR_FAILURE;
  }

  PRUint16 cnt;
  cnt = set->Count();
  return (mCursor >= 0 && mCursor < cnt)
    ? NS_ENUMERATOR_FALSE : NS_OK;
}


////////////////////////////////////////////////////////////
//
// nsWSDLInterfaceSet implementation
//
////////////////////////////////////////////////////////////
#define WSDL_ARENA_BLOCK_SIZE    (1024 * 1)

nsWSDLInterfaceSet::nsWSDLInterfaceSet()
{
  NS_INIT_ISUPPORTS();
  mArena = XPT_NewArena(WSDL_ARENA_BLOCK_SIZE, sizeof(double),
                        "WSDL interface structs");
}

nsWSDLInterfaceSet::~nsWSDLInterfaceSet()
{
  mInterfaces.Clear();
  if (mArena) {
    XPT_DestroyArena(mArena);
  }
}

NS_INTERFACE_MAP_BEGIN(nsWSDLInterfaceSet)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceInfoManager)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceInfoManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

/* nsIInterfaceInfo getInfoForIID (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsWSDLInterfaceSet::GetInfoForIID(const nsIID * iid, 
                                  nsIInterfaceInfo **_retval)
{
  // Since we anticipate a small number of interfaces per manager,
  // do a linear search.
  PRUint32 i, count;
  mInterfaces.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> sup;
    mInterfaces.GetElementAt(i, getter_AddRefs(sup));

    nsWeakPtr weak(do_QueryInterface(sup));
    if (!weak) {
      continue;
    }

    nsCOMPtr<nsIInterfaceInfo> info(do_QueryInterface(weak));
    if (!info) {
      continue;
    }

    nsIID* interfaceIID;
    info->GetIID(&interfaceIID);
    if (interfaceIID->Equals(*iid)) {
      *_retval = info.get();
      NS_ADDREF(*_retval);
      return NS_OK;
    }
  }

  *_retval = nsnull;
  return NS_ERROR_FAILURE;    
}

/* nsIInterfaceInfo getInfoForName (in string name); */
NS_IMETHODIMP 
nsWSDLInterfaceSet::GetInfoForName(const char *name, 
                                   nsIInterfaceInfo **_retval)
{
  // Since we anticipate a small number of interfaces per manager,
  // do a linear search.
  PRUint32 i, count;
  mInterfaces.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> sup;
    mInterfaces.GetElementAt(i, getter_AddRefs(sup));

    nsWeakPtr weak(do_QueryInterface(sup));
    if (!weak) {
      continue;
    }

    nsCOMPtr<nsIInterfaceInfo> info(do_QueryInterface(weak));
    if (!info) {
      continue;
    }

    nsXPIDLCString interfaceName;
    info->GetName(getter_Copies(interfaceName));
    if (interfaceName && nsCRT::strcmp(name, interfaceName.get())) {
      *_retval = info.get();
      NS_ADDREF(*_retval);
      return NS_OK;
    }
  }

  *_retval = nsnull;
  return NS_ERROR_FAILURE;    
}

/* nsIIDPtr getIIDForName (in string name); */
NS_IMETHODIMP 
nsWSDLInterfaceSet::GetIIDForName(const char *name, 
                                  nsIID * *_retval)
{
  nsCOMPtr<nsIInterfaceInfo> info;
  nsresult rv = GetInfoForName(name, getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return info->GetIID(_retval);
}

/* string getNameForIID (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsWSDLInterfaceSet::GetNameForIID(const nsIID * iid, char **_retval)
{
  nsCOMPtr<nsIInterfaceInfo> info;
  nsresult rv = GetInfoForIID(iid, getter_AddRefs(info));
  if (NS_FAILED(rv)) {
    return rv;
  }

  return info->GetName(_retval);
}

/* nsIEnumerator enumerateInterfaces (); */
NS_IMETHODIMP 
nsWSDLInterfaceSet::EnumerateInterfaces(nsIEnumerator **_retval)
{
  nsWSDLInterfaceEnumerator* iter = new nsWSDLInterfaceEnumerator(this);
  if (!iter) {
    return NS_ERROR_FAILURE;
  }
  
  *_retval = iter;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* void autoRegisterInterfaces (); */
NS_IMETHODIMP 
nsWSDLInterfaceSet::AutoRegisterInterfaces()
{
  return NS_OK;
}

nsresult 
nsWSDLInterfaceSet::AppendInterface(nsIInterfaceInfo* aInfo, 
                                    PRUint16* aIndex)
{
  nsWeakPtr weak(do_GetWeakReference(aInfo));
  if (!weak) {
    return NS_ERROR_FAILURE;
  }

  mInterfaces.AppendElement(weak);

  PRUint32 count;
  mInterfaces.Count(&count);
  *aIndex = PRUint16(count-1);
  
  return NS_OK;
}

nsresult
nsWSDLInterfaceSet::GetInterfaceAt(PRUint16 aIndex, nsIInterfaceInfo** aInfo)
{
  nsresult rv;
  if (aIndex == NS_WSDLINTERFACEINFOID_ISUPPORTS) {
    nsCOMPtr<nsIInterfaceInfoManager> manager = do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    static const nsIID iid = NS_ISUPPORTS_IID;
    return manager->GetInfoForIID(&iid, aInfo);
  }
  // XXX Other reserved interface ids should be added here

  PRUint16 index = aIndex - NS_WSDLINTERFACEINFOID_RESERVED_BOUNDARY;
  nsWeakPtr weak;
  mInterfaces.QueryElementAt(aIndex,
                             NS_GET_IID(nsIWeakReference), 
                             getter_AddRefs(weak));
  nsCOMPtr<nsIInterfaceInfo> info(do_QueryReferent(weak));
  if (!info) {
    return NS_ERROR_FAILURE;
  }
  
  *aInfo = info;
  NS_ADDREF(*aInfo);

  return NS_OK;
}
 
PRUint16
nsWSDLInterfaceSet::Count()
{
  // XXX Should the count include reserved interfaces also?
  PRUint32 count;
  mInterfaces.Count(&count);

  return (PRUint16)count;
}


////////////////////////////////////////////////////////////
//
// nsWSDLInterfaceInfo implementation
//
////////////////////////////////////////////////////////////

#define NS_WSDL_PARENT_METHOD_BASE 3

nsWSDLInterfaceInfo::nsWSDLInterfaceInfo(nsAReadableCString& aName, 
                                         const nsIID& aIID,
                                         nsWSDLInterfaceSet* aInterfaceSet)
  : mName(aName), mIID(aIID)
{
  NS_INIT_ISUPPORTS();
  mInterfaceSet = aInterfaceSet;
  NS_ADDREF(mInterfaceSet);
}

nsWSDLInterfaceInfo::~nsWSDLInterfaceInfo()
{
  NS_RELEASE(mInterfaceSet);
  // method descriptors will be deleted when the
  // interface sets arena goes away.
}

NS_INTERFACE_MAP_BEGIN(nsWSDLInterfaceInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceInfo)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

/* readonly attribute string name; */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetName(char * *aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  *aName = mName.ToNewCString();
  if (!*aName) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* readonly attribute nsIIDPtr IID; */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetIID(nsIID * *aIID)
{
  NS_ENSURE_ARG_POINTER(aIID);

  *aIID = (nsIID*)nsMemory::Clone(&mIID, sizeof(nsIID));
  if (!*aIID) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* PRBool isScriptable (); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::IsScriptable(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_TRUE;

  return NS_OK;
}

/* readonly attribute nsIInterfaceInfo parent; */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetParent(nsIInterfaceInfo * *aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  // For WSDL interfaces, the parent is always nsISupports
  return mInterfaceSet->GetInterfaceAt(NS_WSDLINTERFACEINFOID_ISUPPORTS,
                                       aParent);
}

/* readonly attribute PRUint16 methodCount; */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetMethodCount(PRUint16 *aMethodCount)
{
  NS_ENSURE_ARG_POINTER(aMethodCount);

  // Remember to include the nsISupports methods
  *aMethodCount = NS_WSDL_PARENT_METHOD_BASE + 
    PRUint16(mMethodDescriptors.Count());

  return NS_OK;
}

/* readonly attribute PRUint16 constantCount; */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetConstantCount(PRUint16 *aConstantCount)
{
  NS_ENSURE_ARG_POINTER(aConstantCount);

  // XXX Currently WSDL interfaces and nsISupports don't have constants
  *aConstantCount = 0;

  return NS_OK;
}

/* void getMethodInfo (in PRUint16 index, [shared, retval] out nsXPTMethodInfoPtr info); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetMethodInfo(PRUint16 index, 
                                   const nsXPTMethodInfo * *info)
{
  NS_ENSURE_ARG_POINTER(info);

  nsresult rv;
  if (index < NS_WSDL_PARENT_METHOD_BASE) {
    nsCOMPtr<nsIInterfaceInfo> parentInfo;
    
    rv = GetParent(getter_AddRefs(parentInfo));
    if (NS_FAILED(rv)) return rv;
    
    return parentInfo->GetMethodInfo(index, info);
  }

  if (index > (NS_WSDL_PARENT_METHOD_BASE + 
               PRUint16(mMethodDescriptors.Count()))) {
    *info = NULL;
    return NS_ERROR_INVALID_ARG;
  }

  *info = NS_REINTERPRET_CAST(nsXPTMethodInfo*, 
             mMethodDescriptors.ElementAt(index - NS_WSDL_PARENT_METHOD_BASE));

  return NS_OK;
}

/* void getMethodInfoForName (in string methodName, out PRUint16 index, [shared, retval] out nsXPTMethodInfoPtr info); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetMethodInfoForName(const char *methodName, 
                                          PRUint16 *index, 
                                          const nsXPTMethodInfo * *_retval)
{
  NS_ENSURE_ARG(methodName);
  NS_ENSURE_ARG_POINTER(index);
  NS_ENSURE_ARG_POINTER(_retval);

  // This is a slow algorithm, but this is not expected to be called much.
  PRUint32 i, count = mMethodDescriptors.Count();
  for(i = 0; i < count; ++i) {
    const nsXPTMethodInfo* info;
    info = NS_REINTERPRET_CAST(nsXPTMethodInfo*,
                               mMethodDescriptors.ElementAt(i));
    if (PL_strcmp(methodName, info->GetName()) == 0) {
      *index = PRUint16(i) + NS_WSDL_PARENT_METHOD_BASE;
      *_retval = info;
      return NS_OK;
    }
  }

  // If it's not one of ours, ask our parent.
  nsCOMPtr<nsIInterfaceInfo> parentInfo;
  nsresult rv = GetParent(getter_AddRefs(parentInfo));
  if (NS_FAILED(rv)) return rv;
  
  return parentInfo->GetMethodInfoForName(methodName, index, _retval);
}

/* void getConstant (in PRUint16 index, [shared, retval] out nsXPTConstantPtr constant); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetConstant(PRUint16 index, 
                                 const nsXPTConstant * *constant)
{
  NS_ENSURE_ARG_POINTER(constant);
  
  *constant = nsnull;

  return NS_ERROR_INVALID_ARG;
}

/* nsIInterfaceInfo getInfoForParam (in PRUint16 methodIndex, [const] in nsXPTParamInfoPtr param); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetInfoForParam(PRUint16 methodIndex, 
                                     const nsXPTParamInfo * param, 
                                     nsIInterfaceInfo **_retval)
{
  NS_ENSURE_ARG(param);
  NS_ENSURE_ARG_POINTER(_retval);
 
  if (methodIndex < NS_WSDL_PARENT_METHOD_BASE) {
    nsCOMPtr<nsIInterfaceInfo> parentInfo;
    
    nsresult rv = GetParent(getter_AddRefs(parentInfo));
    if (NS_FAILED(rv)) return rv;
    
    return parentInfo->GetInfoForParam(methodIndex, param, _retval);
  }

  if (methodIndex > (NS_WSDL_PARENT_METHOD_BASE + 
                     PRUint16(mMethodDescriptors.Count()))) {
    *_retval = nsnull;
    return NS_ERROR_INVALID_ARG;
  }

  const XPTTypeDescriptor *td = &param->type;

  while (XPT_TDP_TAG(td->prefix) == TD_ARRAY) {
    td = NS_STATIC_CAST(const XPTTypeDescriptor *, 
                        mAdditionalTypes.ElementAt(td->type.additional_type));
  }

  if(XPT_TDP_TAG(td->prefix) != TD_INTERFACE_TYPE) {
    *_retval = nsnull;
    return NS_ERROR_INVALID_ARG;
  }

  return mInterfaceSet->GetInterfaceAt(td->type.iface, _retval);
}

/* nsIIDPtr getIIDForParam (in PRUint16 methodIndex, [const] in nsXPTParamInfoPtr param); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetIIDForParam(PRUint16 methodIndex, 
                                    const nsXPTParamInfo * param, 
                                    nsIID * *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsCOMPtr<nsIInterfaceInfo> info;
  nsresult rv = GetInfoForParam(methodIndex, param, getter_AddRefs(info));
  if(NS_FAILED(rv)) {
    return rv;
  }
  return info->GetIID(_retval);
}

// this is a private helper
nsresult
nsWSDLInterfaceInfo::GetTypeInArray(const nsXPTParamInfo* param,
                                    uint16 dimension,
                                    const XPTTypeDescriptor** type)
{
  const XPTTypeDescriptor *td = &param->type;
  
  for (PRUint16 i = 0; i < dimension; i++) {
    if(XPT_TDP_TAG(td->prefix) != TD_ARRAY) {
      NS_ASSERTION(0, "bad dimension");
      return NS_ERROR_INVALID_ARG;
    }
    td = NS_STATIC_CAST(const XPTTypeDescriptor *, 
                        mAdditionalTypes.ElementAt(td->type.additional_type));
  }
  
  *type = td;
  return NS_OK;
}

/* nsXPTType getTypeForParam (in PRUint16 methodIndex, [const] in nsXPTParamInfoPtr param, in PRUint16 dimension); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetTypeForParam(PRUint16 methodIndex, 
                                     const nsXPTParamInfo * param, 
                                     PRUint16 dimension, 
                                     nsXPTType *_retval)
{
  NS_ENSURE_ARG(param);
  NS_ENSURE_ARG_POINTER(_retval);

  if (methodIndex < NS_WSDL_PARENT_METHOD_BASE) {
    nsCOMPtr<nsIInterfaceInfo> parentInfo;
    
    nsresult rv = GetParent(getter_AddRefs(parentInfo));
    if (NS_FAILED(rv)) return rv;
    
    return parentInfo->GetTypeForParam(methodIndex, param, dimension, _retval);
  }

  if (methodIndex > (NS_WSDL_PARENT_METHOD_BASE + 
                     PRUint16(mMethodDescriptors.Count()))) {
    *_retval = nsnull;
    return NS_ERROR_INVALID_ARG;
  }
  
  const XPTTypeDescriptor *td;
  
  if(dimension) {
    nsresult rv = GetTypeInArray(param, dimension, &td);
    if(NS_FAILED(rv))
      return rv;
  }
  else
    td = &param->type;
  
  *_retval = nsXPTType(td->prefix);
  return NS_OK;
}

/* PRUint8 getSizeIsArgNumberForParam (in PRUint16 methodIndex, [const] in nsXPTParamInfoPtr param, in PRUint16 dimension); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetSizeIsArgNumberForParam(PRUint16 methodIndex, 
                                                const nsXPTParamInfo * param, 
                                                PRUint16 dimension, 
                                                PRUint8 *_retval)
{
  NS_ENSURE_ARG(param);
  NS_ENSURE_ARG_POINTER(_retval);

  if (methodIndex < NS_WSDL_PARENT_METHOD_BASE) {
    nsCOMPtr<nsIInterfaceInfo> parentInfo;
    
    nsresult rv = GetParent(getter_AddRefs(parentInfo));
    if (NS_FAILED(rv)) return rv;
    
    return parentInfo->GetSizeIsArgNumberForParam(methodIndex, 
                                                  param, 
                                                  dimension, 
                                                  _retval);
  }

  if (methodIndex > (NS_WSDL_PARENT_METHOD_BASE + 
                     PRUint16(mMethodDescriptors.Count()))) {
    *_retval = nsnull;
    return NS_ERROR_INVALID_ARG;
  }

  const XPTTypeDescriptor *td;
  
  if(dimension) {
    nsresult rv = GetTypeInArray(param, dimension, &td);
    if(NS_FAILED(rv))
      return rv;
  }
  else {
    td = &param->type;
  }

  // verify that this is a type that has size_is
  switch (XPT_TDP_TAG(td->prefix)) {
    case TD_ARRAY:
    case TD_PSTRING_SIZE_IS:
    case TD_PWSTRING_SIZE_IS:
      break;
    default:
      NS_ASSERTION(0, "not a size_is");
      return NS_ERROR_INVALID_ARG;
  }

  *_retval = td->argnum;
  return NS_OK;
}

/* PRUint8 getLengthIsArgNumberForParam (in PRUint16 methodIndex, [const] in nsXPTParamInfoPtr param, in PRUint16 dimension); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetLengthIsArgNumberForParam(PRUint16 methodIndex, 
                                                  const nsXPTParamInfo * param, 
                                                  PRUint16 dimension, 
                                                  PRUint8 *_retval)
{
  NS_ENSURE_ARG(param);
  NS_ENSURE_ARG_POINTER(_retval);

  if (methodIndex < NS_WSDL_PARENT_METHOD_BASE) {
    nsCOMPtr<nsIInterfaceInfo> parentInfo;
    
    nsresult rv = GetParent(getter_AddRefs(parentInfo));
    if (NS_FAILED(rv)) return rv;
    
    return parentInfo->GetLengthIsArgNumberForParam(methodIndex, 
                                                    param, 
                                                    dimension, 
                                                    _retval);
  }

  if (methodIndex > (NS_WSDL_PARENT_METHOD_BASE + 
                     PRUint16(mMethodDescriptors.Count()))) {
    *_retval = nsnull;
    return NS_ERROR_INVALID_ARG;
  }

  const XPTTypeDescriptor *td;
  
  if(dimension) {
    nsresult rv = GetTypeInArray(param, dimension, &td);
    if(NS_FAILED(rv)) {
      return rv;
    }
  }
  else {
    td = &param->type;
  }

  // verify that this is a type that has length_is
  switch (XPT_TDP_TAG(td->prefix)) {
    case TD_ARRAY:
    case TD_PSTRING_SIZE_IS:
    case TD_PWSTRING_SIZE_IS:
      break;
    default:
      NS_ASSERTION(0, "not a length_is");
      return NS_ERROR_INVALID_ARG;
  }

  *_retval = td->argnum2;
  return NS_OK;
}

/* PRUint8 getInterfaceIsArgNumberForParam (in PRUint16 methodIndex, [const] in nsXPTParamInfoPtr param); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetInterfaceIsArgNumberForParam(PRUint16 methodIndex, 
                                                     const nsXPTParamInfo * param, 
                                                     PRUint8 *_retval)
{
  NS_ENSURE_ARG(param);
  NS_ENSURE_ARG_POINTER(_retval);

  if (methodIndex < NS_WSDL_PARENT_METHOD_BASE) {
    nsCOMPtr<nsIInterfaceInfo> parentInfo;
    
    nsresult rv = GetParent(getter_AddRefs(parentInfo));
    if (NS_FAILED(rv)) return rv;
    
    return parentInfo->GetInterfaceIsArgNumberForParam(methodIndex, 
                                                       param, 
                                                       _retval);
  }

  if (methodIndex > (NS_WSDL_PARENT_METHOD_BASE + 
                     PRUint16(mMethodDescriptors.Count()))) {
    *_retval = nsnull;
    return NS_ERROR_INVALID_ARG;
  }

  const XPTTypeDescriptor *td = &param->type;
  
  while (XPT_TDP_TAG(td->prefix) == TD_ARRAY) {
    td = NS_STATIC_CAST(const XPTTypeDescriptor *, 
                        mAdditionalTypes.ElementAt(td->type.additional_type));
  }
  
  if(XPT_TDP_TAG(td->prefix) != TD_INTERFACE_IS_TYPE) {
    NS_ASSERTION(0, "not an iid_is");
    return NS_ERROR_INVALID_ARG;
  }
  
  *_retval = td->argnum;
  return NS_OK;
}

/* PRBool isIID (in nsIIDPtr IID); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::IsIID(const nsIID * IID, PRBool *_retval)
{
  NS_ENSURE_ARG(IID);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = mIID.Equals(*IID);
  return NS_OK;
}

/* void getNameShared ([shared, retval] out string name); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetNameShared(const char **name)
{
  NS_ENSURE_ARG_POINTER(name);

  *name = mName.get();

  return NS_OK;
}

/* void getIIDShared ([shared, retval] out nsIIDPtrShared iid); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::GetIIDShared(const nsIID * *iid)
{
  NS_ENSURE_ARG_POINTER(iid);

  *iid = &mIID;

  return NS_OK;
}

/* PRBool isFunction (); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::IsFunction(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_FALSE;

  return NS_OK;
}

/* PRBool hasAncestor (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsWSDLInterfaceInfo::HasAncestor(const nsIID * iid, PRBool *_retval)
{
  NS_ENSURE_ARG(iid);
  NS_ENSURE_ARG_POINTER(_retval);

  static const nsIID supportsIID = NS_ISUPPORTS_IID;
  if (iid->Equals(mIID) || iid->Equals(supportsIID)) {
    *_retval = PR_TRUE;
  }
  else {
    *_retval = PR_FALSE;
  }

  return NS_OK;
}

void
nsWSDLInterfaceInfo::AddMethod(XPTMethodDescriptor* aMethod)
{
  mMethodDescriptors.AppendElement(aMethod);
}

void
nsWSDLInterfaceInfo::AddAdditionalType(XPTTypeDescriptor* aType)
{
  mAdditionalTypes.AppendElement(aType);
}
