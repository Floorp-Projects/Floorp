/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   John Bandhauer (jband@netscape.com)
 *   Vidur Apparao (vidur@netscape.com)
 *
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

#include "wspprivate.h"
#include "nsIWSDL.h"
#include "nsIWSDLSOAPBinding.h"
#include "nsISchema.h"
#include "nsISOAPParameter.h"
#include "nsISOAPHeaderBlock.h"
#include "nsISOAPEncoding.h"
#include "nsIComponentManager.h"
#include "nsVoidArray.h"
#include "nsSOAPUtils.h"

WSPProxy::WSPProxy()
{
  NS_INIT_ISUPPORTS();
}

WSPProxy::~WSPProxy()
{
}

nsresult
WSPProxy::Init(nsIWSDLPort* aPort,
               nsIInterfaceInfo* aPrimaryInterface,
               const nsAReadableString& aQualifier,
               PRBool aIsAsync)
{
  mPort = aPort;
  mPrimaryInterface = aPrimaryInterface;
  mQualifier.Assign(aQualifier);
  mIsAsync = aIsAsync;
  
  if (mIsAsync) {
    // Get the completion method info
    const nsXPTMethodInfo* listenerGetter;
    nsresult rv = mPrimaryInterface->GetMethodInfo(4, &listenerGetter);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    const nsXPTParamInfo& listenerParam = listenerGetter->GetParam(0);
    const nsXPTType& type = listenerParam.GetType();
    if (!type.IsInterfacePointer()) {
      return NS_ERROR_FAILURE;
    }
    rv = mPrimaryInterface->GetInfoForParam(4, &listenerParam,
                                      getter_AddRefs(mListenerInterfaceInfo));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMPL_ADDREF(WSPProxy)
NS_IMPL_RELEASE(WSPProxy)

NS_IMETHODIMP
WSPProxy::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if(aIID.Equals(*mIID) || aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsXPTCStubBase*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsIWebServiceProxy))) {
    *aInstancePtr = NS_STATIC_CAST(nsIWebServiceProxy*, this);
    NS_ADDREF_THIS();
    return NS_OK;    
  }
  else if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    *aInstancePtr = NS_STATIC_CAST(nsIClassInfo*, this);
    NS_ADDREF_THIS();
    return NS_OK;    
  }

  return NS_ERROR_NO_INTERFACE;
}

///////////////////////////////////////////////////
//
// Implementation of nsXPTCStubBase methods
//
///////////////////////////////////////////////////
NS_IMETHODIMP
WSPProxy::CallMethod(PRUint16 methodIndex,
                     const nsXPTMethodInfo* info,
                     nsXPTCMiniVariant* params)
{
  nsresult rv;

  if (methodIndex < 3) {
    NS_ERROR("WSPProxy: bad method index");
    return NS_ERROR_FAILURE;
  }

  // The first method in the interface for async callers is the 
  // one to set the async listener
  if (mIsAsync && (methodIndex == 4)) {
    nsISupports* listener = NS_STATIC_CAST(nsISupports*, params[0].val.p);
    mAsyncListener = listener;
    return NS_OK;
  }

  nsCOMPtr<nsIWSDLOperation> operation;
  rv = mPort->GetOperation(methodIndex - 3, getter_AddRefs(operation));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWSDLMessage> input;
  rv = operation->GetInput(getter_AddRefs(input));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Create the call instance
  nsCOMPtr<nsISOAPCall> call = do_CreateInstance(NS_SOAPCALL_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWSDLBinding> binding;
  nsCOMPtr<nsISOAPEncoding> encoding;
  call->GetEncoding(getter_AddRefs(encoding));

  // Get the method name and target object uri
  nsAutoString methodName, targetObjectURI, address;
  rv = operation->GetBinding(getter_AddRefs(binding));
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  nsCOMPtr<nsISOAPOperationBinding> operationBinding = do_QueryInterface(binding, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint16 style;
  operationBinding->GetStyle(&style);
  // If the style is RPC, find the method name and target object URI.
  // If it is document-style, these are both left blank.
  if (style == nsISOAPPortBinding::STYLE_RPC) {
    operation->GetName(methodName);
    rv = input->GetBinding(getter_AddRefs(binding));
    if (NS_FAILED(rv)) {
      return rv;
    }
    nsCOMPtr<nsISOAPMessageBinding> messageBinding = do_QueryInterface(binding, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
    messageBinding->GetNamespace(targetObjectURI);
  }

  // Set the transport URI
  rv = mPort->GetBinding(getter_AddRefs(binding));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsISOAPPortBinding> portBinding = do_QueryInterface(binding, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  portBinding->GetAddress(address);
  rv = call->SetTransportURI(address);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Set up the parameters to the call
  PRUint32 i, partCount;
  input->GetPartCount(&partCount);

  PRUint32 parameterOrderCount;
  operation->GetParameterOrderCount(&parameterOrderCount);

  // The whichParam array stores the index of the parameter that
  // corresponds to each part. The exception to there being a
  // direct correspondence between part and parameter indices is
  // the existence of a parameterOrder attribute for the operation
  // or parameters that are arrays (and have lengths associated
  // with them).
#define MAX_PARTS 128
  PRUint32 whichParam[MAX_PARTS];
  PRUint32 arrayOffset = 0;

  // Iterate through the parts to figure out how many are
  // body vs. header blocks
  nsCOMPtr<nsIWSDLPart> part;
  PRUint32 headerCount = 0, bodyCount = 0;
  for (i = 0; i < partCount; i++) {
    rv = input->GetPart(i, getter_AddRefs(part));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsAutoString paramName;
    part->GetName(paramName);

    // If the part is an array, then 
    if (IsArray(part)) {
      arrayOffset++;
    }

    // If there's a parameter count, then the order of the incoming
    // parameter may be different than the part order.
    if (parameterOrderCount) {
      PRUint32 pindex;
      rv = operation->GetParameterIndex(paramName, &pindex);
      if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
      }
      whichParam[i] = pindex + arrayOffset;
    }
    else {
      whichParam[i] = i + arrayOffset;
    }

    rv = part->GetBinding(getter_AddRefs(binding));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsISOAPPartBinding> partBinding = do_QueryInterface(binding, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    PRUint16 location;
    partBinding->GetLocation(&location);
    if (location == nsISOAPPartBinding::LOCATION_HEADER) {
      headerCount++;
    }
    else if (location == nsISOAPPartBinding::LOCATION_BODY) {
      bodyCount++;
    }
  }

  // Allocate parameter and header blocks
  nsISOAPParameter** bodyBlocks = nsnull;
  if (bodyCount) {
    bodyBlocks = NS_STATIC_CAST(nsISOAPParameter**,
                      nsMemory::Alloc(bodyCount * sizeof(nsISOAPParameter*)));
    if (!bodyBlocks) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (i = 0; i < bodyCount; i++) {
      rv = CallCreateInstance(NS_SOAPPARAMETER_CONTRACTID, &bodyBlocks[i]);
      if (NS_FAILED(rv)) {
        NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, bodyBlocks);
      }
    }
  }
  
  nsISOAPHeaderBlock** headerBlocks = nsnull;
  if (headerCount) {
    headerBlocks = NS_STATIC_CAST(nsISOAPHeaderBlock**,
                   nsMemory::Alloc(headerCount * sizeof(nsISOAPHeaderBlock*)));
    if (!headerBlocks) {
      if (bodyBlocks) {
        NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(bodyCount, bodyBlocks);
      }
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (i = 0; i < headerCount; i++) {
      rv = CallCreateInstance(NS_SOAPHEADERBLOCK_CONTRACTID, &headerBlocks[i]);
      if (NS_FAILED(rv)) {
        if (bodyBlocks) {
          NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(bodyCount, bodyBlocks);
        }
        NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, headerBlocks);
      }
    }
  }

  // Now iterate through the parameters and set up the parameter blocks
  PRUint32 bodyEntry = 0, headerEntry = 0;
  for (i = 0; i < partCount; i++) {
    input->GetPart(i, getter_AddRefs(part));
    part->GetBinding(getter_AddRefs(binding));
    nsCOMPtr<nsISOAPPartBinding> partBinding = do_QueryInterface(binding);
    PRUint16 location;
    partBinding->GetLocation(&location);
    
    nsAutoString paramName;
    part->GetName(paramName);

    nsCOMPtr<nsISOAPBlock> block;
    if (location == nsISOAPPartBinding::LOCATION_HEADER) {
      block = do_QueryInterface(headerBlocks[headerEntry++]);
    }
    else if (location == nsISOAPPartBinding::LOCATION_BODY) {
      block = do_QueryInterface(bodyBlocks[bodyEntry++]);
    }
    
    if (!block) {
      rv = NS_ERROR_FAILURE;
      goto call_method_end;
    }

    // Get the name, namespaceURI and type of the block based on
    // information from the WSDL part. If the schema component
    // associated with the part is an element, these values come
    // from the schema description of the element. If it is a
    // type, then the values are gathered from elsewhere.
    nsCOMPtr<nsISchemaComponent> schemaComponent;
    rv = part->GetSchemaComponent(getter_AddRefs(schemaComponent));
    if (NS_FAILED(rv)) {
      goto call_method_end;
    }

    nsCOMPtr<nsISchemaType> type;
    nsAutoString blockName, blockNamespace;
    
    nsCOMPtr<nsISchemaElement> element = do_QueryInterface(schemaComponent);
    if (!element) {
      rv = element->GetType(getter_AddRefs(type));
      if (NS_FAILED(rv)) {
        goto call_method_end;
      }
      
      element->GetName(blockName);
      element->GetTargetNamespace(blockNamespace);
    }
    else {
      type = do_QueryInterface(schemaComponent);

      blockName.Assign(paramName);
      partBinding->GetNamespace(blockNamespace);
    }

    block->SetName(blockName);
    block->SetNamespaceURI(blockNamespace);
    block->SetSchemaType(type);

    nsAutoString encodingStyle;
    PRUint16 use;

    partBinding->GetUse(&use);
    // XXX Need a way to specify that a block should not be
    // encoded.
    if (use == nsISOAPPartBinding::USE_ENCODED) {
      partBinding->GetEncodingStyle(encodingStyle);
      if (!encodingStyle.IsEmpty()) {
        nsCOMPtr<nsISOAPEncoding> partEncoding;
        encoding->GetAssociatedEncoding(encodingStyle, PR_FALSE,
                                        getter_AddRefs(partEncoding));
        block->SetEncoding(partEncoding);
      }
    }

    const nsXPTParamInfo& paramInfo = info->GetParam(whichParam[i]);
    PRUint32 arrayLength = 0;
    if (IsArray(part)) {
      arrayLength = params[whichParam[i-1]].val.u32;
#ifdef DEBUG
      const nsXPTType& arrayType = paramInfo.GetType();
      NS_ASSERTION(arrayType.IsArray(), "WSPProxy:: array type incorrect");
#endif
    }

    nsCOMPtr<nsIVariant> value;
    rv = ParameterToVariant(mPrimaryInterface, methodIndex,
                            &paramInfo, params[whichParam[i]], 
                            arrayLength, getter_AddRefs(value));
    if (NS_FAILED(rv)) {
      goto call_method_end;
    }

    block->SetValue(value);
  }
  
  // Encode the parameters to the call
  rv = call->Encode(nsISOAPMessage::VERSION_1_2, 
                    methodName, targetObjectURI,
                    headerCount, headerBlocks,
                    bodyCount, bodyBlocks);
  if (NS_FAILED(rv)) {
    goto call_method_end;
  }

  WSPCallContext* cc;
  cc = new WSPCallContext(this, call, methodName, 
                          operation);
  if (!cc) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto call_method_end;
  }

  if (mIsAsync) {
    PRUint8 pcount;
    pcount = info->GetParamCount();
    // There has to be at least one parameter - the retval.
    if (pcount == 0) {
      rv = NS_ERROR_FAILURE;
      goto call_method_end;
    }

#ifdef DEBUG
    // The last one should be the retval.
    const nsXPTParamInfo& retParamInfo = info->GetParam(pcount - 1);
    if (!retParamInfo.IsRetval()) {
      rv = NS_ERROR_FAILURE;
      goto call_method_end;
    }
    
    // It should be an interface pointer
    const nsXPTType& retType = retParamInfo.GetType();
    if (!retType.IsInterfacePointer()) {
      rv = NS_ERROR_FAILURE;
      goto call_method_end;
    }
#endif    

    nsIWebServiceCallContext** retval = NS_STATIC_CAST(nsIWebServiceCallContext**,
                                                       params[pcount-1].val.p);
    if (retval) {
      rv = NS_ERROR_FAILURE;
      goto call_method_end;
    }
    *retval = cc;

    
    rv = cc->CallAsync(methodIndex+1, mAsyncListener);
    if (NS_FAILED(rv)) {
      goto call_method_end;
    }

    mPendingCalls.AppendElement(NS_STATIC_CAST(nsIWebServiceSOAPCallContext*,
                                               cc));
  }
  else {
    rv = cc->CallSync(methodIndex, params);
  }

call_method_end:
  if (bodyBlocks) {
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(bodyCount, bodyBlocks);
  }
  if (headerBlocks) {
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(headerCount, headerBlocks);
  }
  return rv;
}

PRBool
WSPProxy::IsArray(nsIWSDLPart* aPart)
{
  nsCOMPtr<nsISchemaComponent> schemaComponent;
  nsresult rv = aPart->GetSchemaComponent(getter_AddRefs(schemaComponent));
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  nsCOMPtr<nsISchemaType> type;
  nsCOMPtr<nsISchemaElement> element = do_QueryInterface(schemaComponent);
  if (element) {
    rv = element->GetType(getter_AddRefs(type));
    if (NS_FAILED(rv)) {
      return PR_FALSE;
    }
  }
  else {
    type = do_QueryInterface(schemaComponent, &rv);
    if (NS_FAILED(rv)) {
      return PR_FALSE;
    }
  }

  nsAutoString name, ns;
  type->GetName(name);
  type->GetTargetNamespace(ns);

  if (name.Equals(NS_LITERAL_STRING("Array")) &&
      (ns.Equals(*nsSOAPUtils::kSOAPEncURI[nsISOAPMessage::VERSION_1_1]) ||
       ns.Equals(*nsSOAPUtils::kSOAPEncURI[nsISOAPMessage::VERSION_1_2]))) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

nsresult
WSPProxy::ParameterToVariant(nsIInterfaceInfo* aInterfaceInfo,
                             PRUint32 aMethodIndex,
                             const nsXPTParamInfo* aParamInfo,
                             nsXPTCMiniVariant aMiniVariant,
                             PRUint32 aArrayLength,
                             nsIVariant** aVariant)
{
  nsXPTType type;
  nsresult rv = aInterfaceInfo->GetTypeForParam(aMethodIndex, aParamInfo, 
                                                0, &type);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint8 type_tag = type.TagPart();
  nsCOMPtr<nsIInterfaceInfo> iinfo;
  if (type.IsArray()) {
    nsXPTType arrayType;
    rv = aInterfaceInfo->GetTypeForParam(aMethodIndex, aParamInfo,
                                         1, &arrayType);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    if (arrayType.IsInterfacePointer()) {
      rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    return ArrayXPTCMiniVariantToVariant(arrayType.TagPart(),
                                         aMiniVariant, aArrayLength,
                                         iinfo, aVariant);
  }
  else {
    if (type.IsInterfacePointer()) {
      rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    return XPTCMiniVariantToVariant(type_tag, aMiniVariant, iinfo, aVariant);
  }
}

nsresult
WSPProxy::XPTCMiniVariantToVariant(uint8 aTypeTag,
                                   nsXPTCMiniVariant aResult,
                                   nsIInterfaceInfo* aInterfaceInfo,
                                   nsIVariant** aVariant)
{
  nsresult rv;

  nsCOMPtr<nsIWritableVariant> var = do_CreateInstance(NS_VARIANT_CONTRACTID, 
                                                       &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  switch (aTypeTag) {
    case nsXPTType::T_I8:
      var->SetAsInt8(aResult.val.i8);
      break;
    case nsXPTType::T_I16:
      var->SetAsInt16(aResult.val.i16);
      break;
    case nsXPTType::T_I32:
      var->SetAsInt32(aResult.val.i32);
      break;
    case nsXPTType::T_I64:
      var->SetAsInt64(aResult.val.i64);
      break;
    case nsXPTType::T_U8:
      var->SetAsUint8(aResult.val.u8);
      break;
    case nsXPTType::T_U16:
      var->SetAsUint16(aResult.val.u16);
      break;
    case nsXPTType::T_U32:
      var->SetAsUint32(aResult.val.u32);
      break;
    case nsXPTType::T_U64:
      var->SetAsUint64(aResult.val.u64);
      break;
    case nsXPTType::T_FLOAT:
      var->SetAsFloat(aResult.val.f);
      break;
    case nsXPTType::T_DOUBLE:
      var->SetAsDouble(aResult.val.d);
      break;
    case nsXPTType::T_BOOL:
      var->SetAsBool(aResult.val.b);
      break;
    case nsXPTType::T_CHAR:
      var->SetAsChar(aResult.val.c);
      break;
    case nsXPTType::T_WCHAR:
      var->SetAsWChar(aResult.val.wc);
      break;
    case nsXPTType::T_DOMSTRING:
      var->SetAsAString(*((nsAString*)aResult.val.p));
      break;
    case nsXPTType::T_INTERFACE:
    {
      nsISupports* instance = (nsISupports*)aResult.val.p;
      if (instance) {
        // Rewrap an interface instance in a property bag
        nsCOMPtr<WSPComplexTypeWrapper> wrapper;
        rv = WSPComplexTypeWrapper::Create(instance, aInterfaceInfo, 
                                           getter_AddRefs(wrapper));
        if (NS_FAILED(rv)) {
          return rv;
        }
        var->SetAsInterface(NS_GET_IID(nsIPropertyBag), wrapper);
        NS_RELEASE(instance);
      }
      else {
        var->SetAsEmpty();
      }
      
      break;
    }
    default:
      NS_ERROR("Bad attribute type for complex type interface");
      rv = NS_ERROR_FAILURE;
  }

  *aVariant = var;
  NS_ADDREF(*aVariant);

  return rv;
}

nsresult
WSPProxy::ArrayXPTCMiniVariantToVariant(uint8 aTypeTag,
                                        nsXPTCMiniVariant aResult,
                                        PRUint32 aLength,
                                        nsIInterfaceInfo* aInterfaceInfo,
                                        nsIVariant** aVariant)
{   
  nsresult rv;

  nsCOMPtr<nsIWritableVariant> retvar = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aLength) {
    nsIVariant** entries = (nsIVariant**)nsMemory::Alloc(aLength * sizeof(nsIVariant*));
    if (!entries) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    PRUint32 i = 0;
    nsXPTCVariant var;
    void* array = aResult.val.p;
    nsCOMPtr<nsIVariant> element;

#define POPULATE(_t,_v)                                                  \
  PR_BEGIN_MACRO                                                         \
    for(i = 0; i < aLength; i++)                                         \
    {                                                                    \
      var.type = aTypeTag;                                               \
      var.val._v = *((_t*)array + i);                                    \
      rv = XPTCMiniVariantToVariant(aTypeTag, var, aInterfaceInfo,       \
                                    entries+i);                          \
      if (NS_FAILED(rv)) {                                               \
        break;                                                           \
      }                                                                  \
    }                                                                    \
  PR_END_MACRO

    switch (aTypeTag) {
      case nsXPTType::T_I8:            POPULATE(PRInt8, i8);      break; 
      case nsXPTType::T_I16:           POPULATE(PRInt16, i16);    break;
      case nsXPTType::T_I32:           POPULATE(PRInt32, i32);    break;
      case nsXPTType::T_I64:           POPULATE(PRInt64, i64);    break;
      case nsXPTType::T_U8:            POPULATE(PRUint8, u8);     break; 
      case nsXPTType::T_U16:           POPULATE(PRUint16, u16);   break;
      case nsXPTType::T_U32:           POPULATE(PRUint32, u32);   break;
      case nsXPTType::T_U64:           POPULATE(PRUint64, u64);   break;
      case nsXPTType::T_FLOAT:         POPULATE(float, f);        break;
      case nsXPTType::T_DOUBLE:        POPULATE(double, d);       break;
      case nsXPTType::T_BOOL:          POPULATE(PRBool, b);       break;
      case nsXPTType::T_CHAR:          POPULATE(char, c);         break;
      case nsXPTType::T_WCHAR:         POPULATE(PRUnichar, wc);   break;
      case nsXPTType::T_INTERFACE:     POPULATE(nsISupports*, p); break;
    }

#undef POPULATE

    if (NS_SUCCEEDED(rv)) {
      const nsIID& varIID = NS_GET_IID(nsIVariant);
      rv = retvar->SetAsArray(nsIDataType::VTYPE_INTERFACE, 
                              &varIID, 
                              aLength, entries);
    }

    // Even if we failed while converting, we want to release
    // the entries that were already created.
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, entries);
  }
  else {
    retvar->SetAsEmpty();
  }

  if (NS_SUCCEEDED(rv)) {
    *aVariant = retvar;
    NS_ADDREF(*aVariant);
  }

  return rv;
}


nsresult 
WSPProxy::VariantToInParameter(nsIInterfaceInfo* aInterfaceInfo,
                               PRUint32 aMethodIndex,
                               const nsXPTParamInfo* aParamInfo,
                               nsIVariant* aVariant,
                               nsXPTCVariant* aXPTCVariant)
{
  nsXPTType type;
  nsresult rv = aInterfaceInfo->GetTypeForParam(aMethodIndex, aParamInfo, 
                                                0, &type);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint8 type_tag = type.TagPart();
  nsCOMPtr<nsIInterfaceInfo> iinfo;
  if (type.IsArray()) {
    nsXPTType arrayType;
    rv = aInterfaceInfo->GetTypeForParam(aMethodIndex, aParamInfo,
                                         1, &arrayType);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    if (arrayType.IsInterfacePointer()) {
      rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    return VariantToArrayValue(arrayType.TagPart(), aXPTCVariant,
                               iinfo, aVariant);
  }
  else {
    if (type.IsInterfacePointer()) {
      rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    return VariantToValue(type_tag, &aXPTCVariant->val, iinfo, aVariant);
  }
}

nsresult 
WSPProxy::VariantToOutParameter(nsIInterfaceInfo* aInterfaceInfo,
                                PRUint32 aMethodIndex,
                                const nsXPTParamInfo* aParamInfo,
                                nsIVariant* aVariant,
                                nsXPTCMiniVariant* aMiniVariant)
{
  nsXPTType type;
  nsresult rv = aInterfaceInfo->GetTypeForParam(aMethodIndex, aParamInfo, 
                                                0, &type);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint8 type_tag = type.TagPart();
  nsCOMPtr<nsIInterfaceInfo> iinfo;
  if (type.IsArray()) {
    nsXPTType arrayType;
    rv = aInterfaceInfo->GetTypeForParam(aMethodIndex, aParamInfo,
                                         1, &arrayType);
    if (NS_FAILED(rv)) {
      return rv;
    }
    
    if (arrayType.IsInterfacePointer()) {
      rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    return VariantToArrayValue(arrayType.TagPart(), aMiniVariant,
                               iinfo, aVariant);
  }
  else {
    if (type.IsInterfacePointer()) {
      rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo, 
                                           getter_AddRefs(iinfo));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    return VariantToValue(type_tag, aMiniVariant->val.p, iinfo, aVariant);
  }
  return NS_OK;
}

nsresult
WSPProxy::VariantToValue(uint8 aTypeTag,
                         void* aValue,
                         nsIInterfaceInfo* aInterfaceInfo,
                         nsIVariant* aProperty)
{
  nsresult rv;

  switch(aTypeTag) {
    case nsXPTType::T_I8:
      rv = aProperty->GetAsInt8((PRUint8*)aValue);
      break;
    case nsXPTType::T_I16:
      rv = aProperty->GetAsInt16((PRInt16*)aValue);
      break;
    case nsXPTType::T_I32:
      rv = aProperty->GetAsInt32((PRInt32*)aValue);
      break;
    case nsXPTType::T_I64:
      rv = aProperty->GetAsInt64((PRInt64*)aValue);
      break; 
    case nsXPTType::T_U8:
      rv = aProperty->GetAsUint8((PRUint8*)aValue);
      break;
    case nsXPTType::T_U16:
      rv = aProperty->GetAsUint16((PRUint16*)aValue);
      break;
    case nsXPTType::T_U32:
      rv = aProperty->GetAsUint32((PRUint32*)aValue);
      break;
    case nsXPTType::T_U64:
      rv = aProperty->GetAsUint64((PRUint64*)aValue);
      break;
    case nsXPTType::T_FLOAT:
      rv = aProperty->GetAsFloat((float*)aValue);
      break;
    case nsXPTType::T_DOUBLE:
      rv = aProperty->GetAsDouble((double*)aValue);
      break;
    case nsXPTType::T_BOOL:
      rv = aProperty->GetAsBool((PRBool*)aValue);
      break;
    case nsXPTType::T_CHAR:
      rv = aProperty->GetAsChar((char*)aValue);
      break;
    case nsXPTType::T_WCHAR:
      rv = aProperty->GetAsWChar((PRUnichar*)aValue);
      break;
    case nsXPTType::T_DOMSTRING:
      rv = aProperty->GetAsAString(*(nsAString*)aValue);
      break;
    case nsXPTType::T_INTERFACE:
    {
      PRUint16 dataType;
      aProperty->GetDataType(&dataType);
      if (dataType == nsIDataType::VTYPE_EMPTY) {
        *(nsISupports**)aValue = nsnull;
      }
      else {
        nsCOMPtr<nsISupports> sup;
        rv = aProperty->GetAsISupports(getter_AddRefs(sup));
        if (NS_FAILED(rv)) {
          return rv;
        }
        nsCOMPtr<nsIPropertyBag> propBag = do_QueryInterface(sup, &rv);
        if (NS_FAILED(rv)) {
          return rv;
        }
        WSPPropertyBagWrapper* wrapper;
        rv = WSPPropertyBagWrapper::Create(propBag, aInterfaceInfo, &wrapper);
        if (NS_FAILED(rv)) {
          return rv;
        }

        const nsIID* iid;
        aInterfaceInfo->GetIIDShared(&iid);
        rv = wrapper->QueryInterface(*iid, (void**)aValue);
        NS_RELEASE(wrapper);
      }
      break;
    }
    default:
      NS_ERROR("Bad attribute type for complex type interface");
      rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult
WSPProxy::VariantToArrayValue(uint8 aTypeTag,
                              nsXPTCMiniVariant* aResult,
                              nsIInterfaceInfo* aInterfaceInfo,
                              nsIVariant* aProperty)
{
  void* array;
  PRUint16 type;
  PRUint32 count;

  nsIID arrayIID;
  nsresult rv = aProperty->GetAsArray(&type, &arrayIID, &count, &array);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if ((type != nsIDataType::VTYPE_INTERFACE) || 
      (!arrayIID.Equals(NS_GET_IID(nsIVariant)))) {
    return NS_ERROR_FAILURE;
  }
  nsIVariant** variants = (nsIVariant**)array;

  PRUint32 size;
  // Allocate the array
  switch (aTypeTag) {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
      size = sizeof(PRUint8);
      break;
    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
      size = sizeof(PRUint16);
      break;
    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
      size = sizeof(PRUint32);
      break;
    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
      size = sizeof(PRUint64);
      break;
    case nsXPTType::T_FLOAT:
      size = sizeof(float);
      break;
    case nsXPTType::T_DOUBLE:
      size = sizeof(double);
      break;
    case nsXPTType::T_BOOL:
      size = sizeof(PRBool);
      break;
    case nsXPTType::T_CHAR:
      size = sizeof(char);
      break;
    case nsXPTType::T_WCHAR:
      size = sizeof(PRUnichar);
      break;
    case nsXPTType::T_INTERFACE:
      size = sizeof(nsISupports*);
      break;
    default:
      NS_ERROR("Unexpected array type");
      return NS_ERROR_FAILURE;
  }

  void* outptr = nsMemory::Alloc(count * size);
  if (!outptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 i;
  for (i = 0; i < count; i++) {
    rv = VariantToValue(aTypeTag, (void*)((char*)outptr + (i*size)),
                        aInterfaceInfo, variants[i]);
    if (NS_FAILED(rv)) {
      break;
    }
  }

  // Free the variant array passed back to us
  NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(count, variants);

  // If conversion failed, free the allocated structures
  if (NS_FAILED(rv)) {
    if (aTypeTag == nsXPTType::T_INTERFACE) {
      nsMemory::Free(outptr);
    }
    else {
      NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, (nsISupports**)outptr);
    }

    return rv;
  }

  *((PRUint32*)aResult[0].val.p) = count;
  *((void**)aResult[1].val.p) = outptr;

  return NS_OK;
}

NS_IMETHODIMP
WSPProxy::GetInterfaceInfo(nsIInterfaceInfo** info)
{
  NS_ENSURE_ARG_POINTER(info);

  *info = mPrimaryInterface;
  NS_ADDREF(*info);

  return NS_OK;
}

///////////////////////////////////////////////////
//
// Implementation of nsIWebServiceProxy
//
///////////////////////////////////////////////////

/* readonly attribute nsIWSDLPort port; */
NS_IMETHODIMP 
WSPProxy::GetPort(nsIWSDLPort * *aPort)
{
  NS_ENSURE_ARG_POINTER(aPort);
  
  *aPort = mPort;
  NS_IF_ADDREF(*aPort);

  return NS_OK;
}

/* readonly attribute boolean isAsync; */
NS_IMETHODIMP 
WSPProxy::GetIsAsync(PRBool *aIsAsync)
{
  NS_ENSURE_ARG_POINTER(aIsAsync);
  *aIsAsync = mIsAsync;
  return NS_OK;
}

/* readonly attribute AString qualifier; */
NS_IMETHODIMP 
WSPProxy::GetQualifier(nsAString & aQualifier)
{
  aQualifier.Assign(mQualifier);

  return NS_OK;
}

/* readonly attribute nsISimpleEnumerator pendingCalls; */
NS_IMETHODIMP 
WSPProxy::GetPendingCalls(nsISimpleEnumerator * *aPendingCalls)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////
//
// Implementation of nsIClassInfo
//
///////////////////////////////////////////////////

/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] out nsIIDPtr array); */
NS_IMETHODIMP 
WSPProxy::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 2;
  nsIID** iids = NS_STATIC_CAST(nsIID**, nsMemory::Alloc(2 * sizeof(nsIID*)));
  if (!iids) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  iids[0] = NS_STATIC_CAST(nsIID *, nsMemory::Clone(mIID, sizeof(nsIID)));
  const nsIID& wsiid = NS_GET_IID(nsIWebServiceProxy);
  iids[1] = NS_STATIC_CAST(nsIID *, nsMemory::Clone(&wsiid, sizeof(nsIID)));
  
  *array = iids;

  return NS_OK;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
WSPProxy::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
WSPProxy::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
WSPProxy::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
WSPProxy::GetClassID(nsCID * *aClassID)
{
  *aClassID = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
WSPProxy::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
WSPProxy::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
WSPProxy::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  return NS_ERROR_NOT_AVAILABLE;
}

void 
WSPProxy::GetListenerInterfaceInfo(nsIInterfaceInfo** aInfo)
{
  *aInfo = mListenerInterfaceInfo;
  NS_IF_ADDREF(*aInfo);
}
