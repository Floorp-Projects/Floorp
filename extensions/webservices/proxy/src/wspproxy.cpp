/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao (vidur@netscape.com)  (Original author)
 *   John Bandhauer (jband@netscape.com)
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

#include "wspprivate.h"
#include "nsAutoPtr.h"
#include "nsIWSDL.h"
#include "nsIWSDLSOAPBinding.h"
#include "nsISchema.h"
#include "nsISOAPParameter.h"
#include "nsISOAPHeaderBlock.h"
#include "nsISOAPEncoding.h"
#include "nsIComponentManager.h"
#include "nsVoidArray.h"

#define NS_SOAP_1_1_ENCODING_NAMESPACE \
   "http://schemas.xmlsoap.org/soap/encoding/"
#define NS_SOAP_1_2_ENCODING_NAMESPACE \
   "http://www.w3.org/2001/09/soap-encoding"

WSPProxy::WSPProxy()
  : mIID(nsnull)
{
}

WSPProxy::~WSPProxy()
{
}

NS_IMETHODIMP
WSPProxy::Init(nsIWSDLPort* aPort, nsIInterfaceInfo* aPrimaryInterface,
               nsIInterfaceInfoManager* aInterfaceInfoManager,
               const nsAString& aQualifier, PRBool aIsAsync)
{
  NS_ENSURE_ARG(aPort);
  NS_ENSURE_ARG(aPrimaryInterface);

  mPort = aPort;
  mPrimaryInterface = aPrimaryInterface;
  mInterfaceInfoManager = aInterfaceInfoManager;
  mPrimaryInterface->GetIIDShared(&mIID);
  mQualifier.Assign(aQualifier);
  mIsAsync = aIsAsync;

  nsresult rv;

  mInterfaces = do_CreateInstance(NS_SCRIPTABLE_INTERFACES_CONTRACTID, &rv);
  if (!mInterfaces) {
    return rv;
  }

  rv = mInterfaces->SetManager(mInterfaceInfoManager);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mIsAsync) {
    // Get the completion method info
    const nsXPTMethodInfo* listenerGetter;
    rv = mPrimaryInterface->GetMethodInfo(3, &listenerGetter);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    const nsXPTParamInfo& listenerParam = listenerGetter->GetParam(0);
    const nsXPTType& type = listenerParam.GetType();
    if (!type.IsInterfacePointer()) {
      return NS_ERROR_FAILURE;
    }
    rv = mPrimaryInterface->GetInfoForParam(3, &listenerParam,
                                      getter_AddRefs(mListenerInterfaceInfo));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_METHOD
WSPProxy::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  NS_ENSURE_NO_AGGREGATION(outer);

  WSPProxy* proxy = new WSPProxy();
  if (!proxy) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(proxy);
  nsresult rv = proxy->QueryInterface(aIID, aInstancePtr);
  NS_RELEASE(proxy);
  return rv;
}

NS_IMPL_ADDREF(WSPProxy)
NS_IMPL_RELEASE(WSPProxy)

NS_IMETHODIMP
WSPProxy::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if((mIID && aIID.Equals(*mIID)) || aIID.Equals(NS_GET_IID(nsISupports))) {
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
  nsCOMPtr<nsIWebServiceCallContext> cc;
  nsCOMPtr<nsIWSDLBinding> binding;

  if (methodIndex < 3) {
    NS_ERROR("WSPProxy: bad method index");
    return NS_ERROR_FAILURE;
  }

  // The first method in the interface for async callers is the
  // one to set the async listener
  if (mIsAsync && (methodIndex == 3)) {
    nsISupports* listener = NS_STATIC_CAST(nsISupports*, params[0].val.p);
    mAsyncListener = listener;
    return NS_OK;
  }

  PRUint32 methodOffset;
  if (mIsAsync) {
    methodOffset = 4;
  }
  else {
    methodOffset = 3;
  }

  nsCOMPtr<nsIWSDLOperation> operation;
  rv = mPort->GetOperation(methodIndex - methodOffset,
                           getter_AddRefs(operation));
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

  nsCOMPtr<nsISOAPEncoding> encoding =
    do_CreateInstance(NS_SOAPENCODING_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  call->SetEncoding(encoding);

  // Get the method name and target object uri
  nsAutoString methodName, targetObjectURI;
  rv = operation->GetBinding(getter_AddRefs(binding));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISOAPOperationBinding> operationBinding =
    do_QueryInterface(binding, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString soapAction;
  operationBinding->GetSoapAction(soapAction);
  call->SetActionURI(soapAction);

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
    nsCOMPtr<nsISOAPMessageBinding> messageBinding =
      do_QueryInterface(binding, &rv);
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

  nsAutoString address;
  portBinding->GetAddress(address);
  rv = call->SetTransportURI(address);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint16 version;
  portBinding->GetSoapVersion(&version);
  if (version == nsISOAPMessage::VERSION_UNKNOWN) {
    version = nsISOAPMessage::VERSION_1_1;
  }

  // Set up the parameters to the call
  PRUint32 i, partCount;
  input->GetPartCount(&partCount);

  PRUint32 maxParamIndex = info->GetParamCount()-1;

  // Iterate through the parts to figure out how many are
  // body vs. header blocks
  nsCOMPtr<nsIWSDLPart> part;
  PRUint32 headerCount = 0, bodyCount = 0;

  for (i = 0; i < partCount; i++) {
    rv = input->GetPart(i, getter_AddRefs(part));
    if (NS_FAILED(rv)) {
      return rv;
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

        return rv;
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

        return rv;
      }
    }
  }

  // Now iterate through the parameters and set up the parameter blocks
  PRUint32 bodyEntry = 0, headerEntry = 0, paramIndex = 0;
  for (i = 0; i < partCount; paramIndex++, i++) {
    input->GetPart(i, getter_AddRefs(part));
    part->GetBinding(getter_AddRefs(binding));
    nsCOMPtr<nsISOAPPartBinding> partBinding = do_QueryInterface(binding);
    PRUint16 location;
    partBinding->GetLocation(&location);

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
    if (element) {
      rv = element->GetType(getter_AddRefs(type));
      if (NS_FAILED(rv)) {
        goto call_method_end;
      }

      element->GetName(blockName);
      element->GetTargetNamespace(blockNamespace);
    }
    else {
      type = do_QueryInterface(schemaComponent);

      nsAutoString paramName;
      part->GetName(paramName);

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

    // Look ahead in the param info array to see if the current part has to be
    // treated as an array. If so then get the array length from the current
    // param and increment the param index.

    PRUint32 arrayLength;

    if (paramIndex < maxParamIndex &&
        info->GetParam((PRUint8)(paramIndex + 1)).GetType().IsArray()) {
      arrayLength = params[paramIndex++].val.u32;
    }
    else {
      arrayLength = 0;
    }

    NS_ASSERTION(paramIndex <= maxParamIndex,
                 "WSDL/IInfo param count mismatch");

    const nsXPTParamInfo& paramInfo = info->GetParam(paramIndex);

    nsCOMPtr<nsIVariant> value;
    rv = ParameterToVariant(mPrimaryInterface, methodIndex,
                            &paramInfo, params[paramIndex],
                            arrayLength, getter_AddRefs(value));
    if (NS_FAILED(rv)) {
      goto call_method_end;
    }

    block->SetValue(value);
  }

  // Encode the parameters to the call
  rv = call->Encode(version,
                    methodName, targetObjectURI,
                    headerCount, headerBlocks,
                    bodyCount, bodyBlocks);
  if (NS_FAILED(rv)) {
    goto call_method_end;
  }

  WSPCallContext* ccInst;
  ccInst = new WSPCallContext(this, call, methodName, operation);
  if (!ccInst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto call_method_end;
  }
  cc = ccInst;

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

    nsIWebServiceCallContext** retval =
      NS_STATIC_CAST(nsIWebServiceCallContext**, params[pcount-1].val.p);
    if (!retval) {
      rv = NS_ERROR_FAILURE;
      goto call_method_end;
    }
    *retval = cc;
    NS_ADDREF(*retval);

    rv = ccInst->CallAsync(methodIndex, mAsyncListener);
    if (NS_FAILED(rv)) {
      goto call_method_end;
    }

    mPendingCalls.AppendObject(ccInst);
  }
  else {
    rv = ccInst->CallSync(methodIndex, params);
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

nsresult
WSPProxy::ParameterToVariant(nsIInterfaceInfo* aInterfaceInfo,
                             PRUint32 aMethodIndex,
                             const nsXPTParamInfo* aParamInfo,
                             nsXPTCMiniVariant aMiniVariant,
                             PRUint32 aArrayLength, nsIVariant** aVariant)
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
    return ArrayXPTCMiniVariantToVariant(arrayType.TagPart(), aMiniVariant,
                                         aArrayLength, iinfo, aVariant);
  }

  // else

  if (type.IsInterfacePointer()) {
    rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo,
                                         getter_AddRefs(iinfo));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return XPTCMiniVariantToVariant(type_tag, aMiniVariant, iinfo, aVariant);
}

nsresult
WSPProxy::WrapInPropertyBag(nsISupports* aInstance,
                            nsIInterfaceInfo* aInterfaceInfo,
                            nsIPropertyBag** aPropertyBag)
{
  *aPropertyBag = nsnull;
  nsresult rv;
  nsCOMPtr<nsIWebServiceComplexTypeWrapper> wrapper =
    do_CreateInstance(NS_WEBSERVICECOMPLEXTYPEWRAPPER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = wrapper->Init(aInstance, aInterfaceInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return CallQueryInterface(wrapper, aPropertyBag);
}

nsresult
WSPProxy::XPTCMiniVariantToVariant(uint8 aTypeTag, nsXPTCMiniVariant aResult,
                                   nsIInterfaceInfo* aInterfaceInfo,
                                   nsIVariant** aVariant)
{
  nsresult rv;

  // If a variant is passed in, just return it as is.
  if (aTypeTag == nsXPTType::T_INTERFACE) {
    nsISupports* inst = (nsISupports*)aResult.val.p;
    nsCOMPtr<nsIVariant> instVar = do_QueryInterface(inst);
    if (instVar) {
      *aVariant = instVar;
      NS_ADDREF(*aVariant);
      return NS_OK;
    }
  }

  nsCOMPtr<nsIWritableVariant> var =
    do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
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
    case nsXPTType::T_CHAR_STR:
      var->SetAsString(NS_STATIC_CAST(char*, aResult.val.p));
      break;
    case nsXPTType::T_WCHAR_STR:
      var->SetAsWString(NS_STATIC_CAST(PRUnichar*, aResult.val.p));
      break;
    case nsXPTType::T_DOMSTRING:
    case nsXPTType::T_ASTRING:
      var->SetAsAString(*((nsAString*)aResult.val.p));
      break;
    case nsXPTType::T_INTERFACE:
    {
      nsISupports* instance = (nsISupports*)aResult.val.p;
      if (instance) {
        nsCOMPtr<nsIPropertyBag> propBag;
        rv = WrapInPropertyBag(instance, aInterfaceInfo,
                               getter_AddRefs(propBag));
        if (NS_FAILED(rv)) {
          return rv;
        }
        var->SetAsInterface(NS_GET_IID(nsIPropertyBag), propBag);
        // AFAICT, there is no need to release the instance here 
        // because the caller who owns the object should be releasing it.
        // NS_RELEASE(instance); 
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

  nsCOMPtr<nsIWritableVariant> retvar =
    do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aLength) {
    PRUint32 i = 0;
    void* array = aResult.val.p;
    void* entries;
    nsISupports** entriesSup = nsnull;
    const nsIID* iid = nsnull;

    switch (aTypeTag) {
      case nsXPTType::T_I8:
      case nsXPTType::T_I16:
      case nsXPTType::T_I32:
      case nsXPTType::T_I64:
      case nsXPTType::T_U8:
      case nsXPTType::T_U16:
      case nsXPTType::T_U32:
      case nsXPTType::T_U64:
      case nsXPTType::T_FLOAT:
      case nsXPTType::T_DOUBLE:
      case nsXPTType::T_BOOL:
      case nsXPTType::T_CHAR:
      case nsXPTType::T_WCHAR:
      case nsXPTType::T_CHAR_STR:
      case nsXPTType::T_WCHAR_STR:
        entries = array;
        break;
      case nsXPTType::T_INTERFACE:
      {
        aInterfaceInfo->GetIIDShared(&iid);
        if (iid->Equals(NS_GET_IID(nsIVariant))) {
          entries = array;
        }
        else {
          entriesSup = (nsISupports**)nsMemory::Alloc(aLength *
                                                      sizeof(nsISupports*));
          if (!entriesSup) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          const nsIID& propbagIID = NS_GET_IID(nsIPropertyBag);
          iid = &propbagIID;
          entries = (void*)entriesSup;
          for(i = 0; i < aLength; i++) {
            nsISupports* instance = *((nsISupports**)array + i);
            nsISupports** outptr = entriesSup + i;
            if (instance) {
              nsCOMPtr<nsIPropertyBag> propBag;
              rv = WrapInPropertyBag(instance, aInterfaceInfo,
                                     getter_AddRefs(propBag));
              if (NS_FAILED(rv)) {
                break;
              }
              propBag->QueryInterface(NS_GET_IID(nsISupports),
                                      (void**)outptr);
            }
            else {
              *outptr = nsnull;
            }
          }
        }
        aTypeTag = nsXPTType::T_INTERFACE_IS;
        break;
      }
      default:
        NS_ERROR("Conversion of illegal array type");
        return NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(rv)) {
      rv = retvar->SetAsArray(aTypeTag, iid, aLength, entries);
    }

    if (entriesSup) {
      NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, entriesSup);
    }
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

    aXPTCVariant[0].type = nsXPTType::T_U32;
    aXPTCVariant[1].type = nsXPTType::T_ARRAY;
    aXPTCVariant[1].SetValIsArray();
    return VariantToArrayValue(arrayType.TagPart(), aXPTCVariant, aXPTCVariant+1,
                               iinfo, aVariant);
  }
  // Set the param's type on the XPTCVariant because xptcinvoke's 
  // invoke_copy_to_stack depends on it. This fixes bug 203434.
  aXPTCVariant->type = type;
  // else
  if (type.IsInterfacePointer()) {
    rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo, 
                                         getter_AddRefs(iinfo));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (type_tag == nsXPTType::T_DOMSTRING) {
    // T_DOMSTRING values are expected to be stored in an nsAString
    // object pointed to by the nsXPTCVariant...
    return VariantToValue(type_tag, aXPTCVariant->val.p, iinfo, aVariant);
  }
  // else

  // ... but other types are expected to be stored directly in the
  // variant itself.
  return VariantToValue(type_tag, &aXPTCVariant->val, iinfo, aVariant);
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
    return VariantToArrayValue(arrayType.TagPart(), 
                               aMiniVariant, aMiniVariant + 1,
                               iinfo, aVariant);
  }
  // else
  if (type.IsInterfacePointer()) {
    rv = aInterfaceInfo->GetInfoForParam(aMethodIndex, aParamInfo,
                                         getter_AddRefs(iinfo));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return VariantToValue(type_tag, aMiniVariant->val.p, iinfo, aVariant);
}

nsresult
WSPProxy::WrapInComplexType(nsIPropertyBag* aPropertyBag,
                            nsIInterfaceInfo* aInterfaceInfo,
                            nsISupports** aComplexType)
{
  *aComplexType = nsnull;
  nsRefPtr<WSPPropertyBagWrapper> wrapper = new WSPPropertyBagWrapper();
  if (!wrapper) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = wrapper->Init(aPropertyBag, aInterfaceInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  wrapper->QueryInterface(NS_GET_IID(nsISupports), (void**)aComplexType);
  return NS_OK;
}

nsresult
WSPProxy::VariantToValue(uint8 aTypeTag, void* aValue,
                         nsIInterfaceInfo* aInterfaceInfo,
                         nsIVariant* aProperty)
{
  nsresult rv = NS_OK;

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
    case nsXPTType::T_CHAR_STR:
      rv = aProperty->GetAsString((char**)aValue);
      break;
    case nsXPTType::T_WCHAR_STR:
      rv = aProperty->GetAsWString((PRUnichar**)aValue);
      break;
    case nsXPTType::T_DOMSTRING:
    case nsXPTType::T_ASTRING:
      rv = aProperty->GetAsAString(*(nsAString*)aValue);
      break;
    case nsXPTType::T_INTERFACE:
    {
      const nsIID* iid;
      aInterfaceInfo->GetIIDShared(&iid);
      PRUint16 dataType;
      aProperty->GetDataType(&dataType);
      if (dataType == nsIDataType::VTYPE_EMPTY) {
        *(nsISupports**)aValue = nsnull;
      }
      else if (iid->Equals(NS_GET_IID(nsIVariant))) {
        *(nsIVariant**)aValue = aProperty;
        NS_ADDREF(*(nsIVariant**)aValue);
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
        nsCOMPtr<nsISupports> wrapper;
        rv = WrapInComplexType(propBag, aInterfaceInfo,
                               getter_AddRefs(wrapper));
        if (NS_FAILED(rv)) {
          return rv;
        }

        rv = wrapper->QueryInterface(*iid, (void**)aValue);
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
                              nsXPTCMiniVariant* aResultSize,
                              nsXPTCMiniVariant* aResultArray,
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

  aResultSize->val.u32 = count;

  switch (aTypeTag) {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
    case nsXPTType::T_FLOAT:
    case nsXPTType::T_DOUBLE:
    case nsXPTType::T_BOOL:
    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
    case nsXPTType::T_CHAR_STR:
    case nsXPTType::T_WCHAR_STR:
      aResultArray->val.p = array;
      break;
    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      if (arrayIID.Equals(NS_GET_IID(nsIVariant))) {
        aResultArray->val.p = array;
      }
      else if (!arrayIID.Equals(NS_GET_IID(nsIPropertyBag))) {
        NS_ERROR("Array of complex types should be represented by property "
                 "bags");
        return NS_ERROR_FAILURE;
      }
      else {
        nsISupports** outptr =
          (nsISupports**)nsMemory::Alloc(count * sizeof(nsISupports*));
        if (!outptr) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        nsISupports** arraySup = (nsISupports**)array;
        const nsIID* iid;
        aInterfaceInfo->GetIIDShared(&iid);
        PRUint32 i;
        for (i = 0; i < count; i++) {
          nsCOMPtr<nsIPropertyBag> propBag(do_QueryInterface(arraySup[i]));
          if (!propBag) {
            *(outptr + i) = nsnull;
          }
          else {
            nsCOMPtr<nsISupports> wrapper;
            rv = WrapInComplexType(propBag, aInterfaceInfo,
                                   getter_AddRefs(wrapper));
            if (NS_FAILED(rv)) {
              NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, outptr);
              return rv;
            }
            rv = wrapper->QueryInterface(*iid, (void**)(outptr + i));
            if (NS_FAILED(rv)) {
              NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(i, outptr);
              return rv;
            }
          }
        }
        aResultArray->val.p = outptr;
      }
      break;
    }
    default:
      NS_ERROR("Conversion of illegal array type");
      return NS_ERROR_FAILURE;
  }

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

nsresult
WSPProxy::GetInterfaceName(PRBool listener, char** retval)
{
  if (!mPrimaryInterface) {
    return NS_ERROR_FAILURE;
  }

  const char* rawName;
  nsresult rv = mPrimaryInterface->GetNameShared(&rawName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCAutoString name;

  if (listener) {
    if (mIsAsync) {
      name.Assign(rawName, strlen(rawName) - (sizeof("Async")-1) );
    }
    else {
      name.Assign(rawName);
    }
    name.Append("Listener");
  }
  else {
    name.Assign(rawName);
  }

  *retval = (char*) nsMemory::Clone(name.get(), name.Length()+1);
  return *retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute string primaryInterfaceName; */
NS_IMETHODIMP
WSPProxy::GetPrimaryInterfaceName(char * *aPrimaryInterfaceName)
{
  return GetInterfaceName(PR_FALSE, aPrimaryInterfaceName);
}

/* readonly attribute string primaryAsyncListenerInterfaceName; */
NS_IMETHODIMP
WSPProxy::GetPrimaryAsyncListenerInterfaceName(char * *aPrimaryAsyncListenerInterfaceName)
{
  return GetInterfaceName(PR_TRUE, aPrimaryAsyncListenerInterfaceName);
}

/* readonly attribute nsIScriptableInterfaces interfaces; */
NS_IMETHODIMP
WSPProxy::GetInterfaces(nsIScriptableInterfaces * *aInterfaces)
{
  *aInterfaces = mInterfaces;
  NS_IF_ADDREF(*aInterfaces);
  return NS_OK;
}

///////////////////////////////////////////////////
//
// Implementation of nsIClassInfo
//
///////////////////////////////////////////////////

/* void getInterfaces (out PRUint32 count, [array, size_is (count),
   retval] out nsIIDPtr array); */
NS_IMETHODIMP
WSPProxy::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  if (!mIID) {
    return NS_ERROR_NOT_INITIALIZED;
  }

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

void
WSPProxy::CallCompleted(WSPCallContext* aContext)
{
  mPendingCalls.RemoveObject(aContext);
}
