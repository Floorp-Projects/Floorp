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
#include "nsIWSDL.h"
#include "nsISOAPCall.h"
#include "nsISOAPHeaderBlock.h"
#include "nsISOAPParameter.h"
#include "nsIWSDLSOAPBinding.h"

WSPCallContext::WSPCallContext(WSPProxy* aProxy, nsISOAPCall* aSOAPCall,
                               const nsAString& aMethodName,
                               nsIWSDLOperation* aOperation)
  : mProxy(aProxy), mCall(aSOAPCall), mMethodName(aMethodName),
    mOperation(aOperation), mStatus(NS_ERROR_NOT_AVAILABLE)
{
  NS_IF_ADDREF(mProxy);
}

WSPCallContext::~WSPCallContext()
{
  NS_IF_RELEASE(mProxy);
}

NS_IMPL_ISUPPORTS3_CI(WSPCallContext,
                      nsIWebServiceCallContext,
                      nsIWebServiceSOAPCallContext,
                      nsISOAPResponseListener)

/* readonly attribute nsIWebServiceProxy proxy; */
NS_IMETHODIMP
WSPCallContext::GetProxy(nsIWebServiceProxy * *aProxy)
{
  NS_ENSURE_ARG_POINTER(aProxy);

  *aProxy = mProxy;
  NS_IF_ADDREF(*aProxy);

  return NS_OK;
}

/* readonly attribute AString methodName; */
NS_IMETHODIMP
WSPCallContext::GetMethodName(nsAString & aMethodName)
{
  aMethodName.Assign(mMethodName);
  return NS_OK;
}

/* readonly attribute PRUint32 status; */
NS_IMETHODIMP
WSPCallContext::GetStatus(PRUint32 *aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  *aStatus = mStatus;
  return NS_OK;
}

/* readonly attribute nsIException pendingException; */
NS_IMETHODIMP
WSPCallContext::GetPendingException(nsIException * *aPendingException)
{
  NS_ENSURE_ARG_POINTER(aPendingException);
  *aPendingException = mException;
  NS_IF_ADDREF(*aPendingException);
  return NS_OK;
}

/* readonly attribute nsIWSDLOperation operation; */
NS_IMETHODIMP
WSPCallContext::GetOperation(nsIWSDLOperation * *aOperation)
{
  NS_ENSURE_ARG_POINTER(aOperation);
  *aOperation = mOperation;
  NS_IF_ADDREF(*aOperation);
  return NS_OK;
}

/* void abort (in nsIException error); */
NS_IMETHODIMP
WSPCallContext::Abort(nsIException *error)
{
  nsresult rv = NS_OK;
  if (mCompletion) {
    mException = error;
    PRBool ret;
    rv = mCompletion->Abort(&ret);
    if (NS_SUCCEEDED(rv) && ret) {
      rv = CallCompletionListener();
    }
    // Drop our ref to the completion object to break our cycle with
    // it.
    mCompletion = nsnull;
  }
  return rv;
}

/* readonly attribute nsISOAPResponse soapResponse; */
NS_IMETHODIMP
WSPCallContext::GetSoapResponse(nsISOAPResponse * *aSoapResponse)
{
  NS_ENSURE_ARG_POINTER(aSoapResponse);
  NS_IF_ADDREF(*aSoapResponse = mResponse);
  return NS_OK;
}

/* boolean handleResponse (in nsISOAPResponse aResponse, in
   nsISOAPCall aCall, in nsresult status, in boolean aLast); */
NS_IMETHODIMP
WSPCallContext::HandleResponse(nsISOAPResponse *aResponse, nsISOAPCall *aCall,
                               nsresult status, PRBool aLast, PRBool *_retval)
{
  NS_ASSERTION(aCall == mCall, "unexpected call instance");
  NS_ASSERTION(aLast, "only single response expected");
  mStatus = status;
  *_retval = PR_TRUE;
  CallCompletionListener();
  // Drop our ref to the completion object now that we dont need it.
  // This breaks our reference cycle with it and prevents leaks.
  mCompletion = nsnull;
  return NS_OK;
}

nsresult
WSPCallContext::CallAsync(PRUint32 aListenerMethodIndex,
                          nsISupports* aListener)
{
  mAsyncListener = aListener;
  mListenerMethodIndex = aListenerMethodIndex;
  return mCall->AsyncInvoke(this, getter_AddRefs(mCompletion));
}

nsresult
WSPCallContext::CallSync(PRUint32 aMethodIndex, nsXPTCMiniVariant* params)
{
  return mCall->Invoke(getter_AddRefs(mResponse));
}

nsresult
WSPCallContext::CallCompletionListener()
{
  nsresult rv;
#define PARAM_BUFFER_COUNT     8  /* Never set less than 2 */

  if (!mProxy) {
    NS_ERROR("Huh, no proxy?");

    return NS_OK;
  }
  nsXPTCVariant paramBuffer[PARAM_BUFFER_COUNT];
  nsXPTCVariant* dispatchParams = nsnull;

  nsCOMPtr<nsISOAPFault> fault;
  mCompletion->GetResponse(getter_AddRefs(mResponse));
  if (mResponse) {
    rv = mResponse->GetFault(getter_AddRefs(fault));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  if (!mResponse || fault) {
    WSPException* exception = new WSPException(fault, mStatus);
    if (!exception) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mException = exception;
  }

  nsCOMPtr<nsIInterfaceInfo> listenerInterfaceInfo;
  mProxy->GetListenerInterfaceInfo(getter_AddRefs(listenerInterfaceInfo));
  NS_ASSERTION(listenerInterfaceInfo,
               "WSPCallContext:Missing listener interface info");

  const nsXPTMethodInfo* methodInfo;
  rv = listenerInterfaceInfo->GetMethodInfo(mListenerMethodIndex,
                                            &methodInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint32 paramCount = methodInfo->GetParamCount();
  if(paramCount > PARAM_BUFFER_COUNT) {
    if(!(dispatchParams = new nsXPTCVariant[paramCount])) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    dispatchParams = paramBuffer;
  }

  // iterate through the params to clear flags
  PRUint32 i;
  for(i = 0; i < paramCount; i++) {
    nsXPTCVariant* dp = &dispatchParams[i];
    dp->ClearFlags();
    dp->val.p = nsnull;
  }

  PRUint32 headerCount = 0, bodyCount = 0;
  nsISOAPHeaderBlock** headerBlocks;
  nsISOAPParameter** bodyBlocks;

#define STRING_ARRAY_BUF_SIZE 2
  nsAutoString string_array_buf[STRING_ARRAY_BUF_SIZE];
  nsAutoString *string_array = &string_array_buf[0];
  PRUint32 string_array_index = 0;


  // If we have an exception, report it now
  if (mException) {
    nsCOMPtr<nsISupports> canonical_this = 
      do_QueryInterface(NS_STATIC_CAST(nsIWebServiceCallContext*, this));
    dispatchParams[0].val.p = mException.get();
    dispatchParams[0].SetValIsInterface();
    dispatchParams[0].type.flags = XPT_TDP_POINTER | TD_INTERFACE_TYPE;

    dispatchParams[1].val.p = canonical_this;
    dispatchParams[1].SetValIsInterface();
    dispatchParams[1].type.flags = XPT_TDP_POINTER | TD_INTERFACE_TYPE;

    rv = XPTC_InvokeByIndex(mAsyncListener, 3, 2, dispatchParams);
  }
  else if (mResponse) {
    nsCOMPtr<nsIWSDLBinding> binding;
    rv = mOperation->GetBinding(getter_AddRefs(binding));
    if (NS_FAILED(rv)) {
      goto call_completion_end;
    }

    nsCOMPtr<nsISOAPOperationBinding> operationBinding =
      do_QueryInterface(binding, &rv);
    if (NS_FAILED(rv)) {
      goto call_completion_end;
    }

    PRUint16 style;
    operationBinding->GetStyle(&style);

    rv = mResponse->GetHeaderBlocks(&headerCount, &headerBlocks);
    if (NS_FAILED(rv)) {
      goto call_completion_end;
    }
    rv = mResponse->GetParameters(style == nsISOAPPortBinding::STYLE_DOCUMENT,
                                  &bodyCount, &bodyBlocks);
    if (NS_FAILED(rv)) {
      goto call_completion_end;
    }

    nsCOMPtr<nsIWSDLMessage> output;
    rv = mOperation->GetOutput(getter_AddRefs(output));
    if (NS_FAILED(rv)) {
      goto call_completion_end;
    }
    PRUint32 partCount;
    output->GetPartCount(&partCount);

    PRUint32 maxParamIndex = methodInfo->GetParamCount()-1;

    PRUint32 bodyEntry = 0, headerEntry = 0, paramIndex = 0;
    for (i = 0; i < partCount; paramIndex++, i++) {
      nsCOMPtr<nsIWSDLPart> part;
      rv = output->GetPart(i, getter_AddRefs(part));
      if (NS_FAILED(rv)) {
        goto call_completion_end;
      }
      rv = part->GetBinding(getter_AddRefs(binding));
      if (NS_FAILED(rv)) {
        goto call_completion_end;
      }

      nsCOMPtr<nsISOAPPartBinding> partBinding =
        do_QueryInterface(binding, &rv);
      if (NS_FAILED(rv)) {
        goto call_completion_end;
      }

      PRUint16 location;
      partBinding->GetLocation(&location);

      nsCOMPtr<nsISOAPBlock> block;
      if (location == nsISOAPPartBinding::LOCATION_HEADER && 
          headerEntry < headerCount) {
        block = do_QueryInterface(headerBlocks[headerEntry++]);
      }
      else if (location == nsISOAPPartBinding::LOCATION_BODY &&
               bodyEntry < bodyCount) {
        block = do_QueryInterface(bodyBlocks[bodyEntry++]);
      }

      if (!block) {
        rv = NS_ERROR_UNEXPECTED;
        goto call_completion_end;
      }

      nsCOMPtr<nsISchemaComponent> schemaComponent;
      rv = part->GetSchemaComponent(getter_AddRefs(schemaComponent));
      if (NS_FAILED(rv)) {
        goto call_completion_end;
      }

      nsCOMPtr<nsISchemaType> type;
      nsCOMPtr<nsISchemaElement> element = do_QueryInterface(schemaComponent);
      if (element) {
        rv = element->GetType(getter_AddRefs(type));
        if (NS_FAILED(rv)) {
          goto call_completion_end;
        }
      }
      else {
        type = do_QueryInterface(schemaComponent);
      }

      rv = block->SetSchemaType(type);
      if (NS_FAILED(rv)) {
        goto call_completion_end;
      }

      nsCOMPtr<nsIVariant> value;
      rv = block->GetValue(getter_AddRefs(value));
      if (NS_FAILED(rv) || !value) {
        goto call_completion_end;
      }

      nsXPTCVariant* vars = dispatchParams + paramIndex;

      if (paramIndex < maxParamIndex &&
          methodInfo->GetParam((PRUint8)(paramIndex+1)).GetType().IsArray()) {
        paramIndex++;
      }

      NS_ASSERTION(paramIndex <= maxParamIndex,
                   "WSDL/IInfo param count mismatch");

      const nsXPTParamInfo& paramInfo = methodInfo->GetParam(paramIndex);
      
      if (XPT_TDP_TAG(paramInfo.type.prefix) == TD_DOMSTRING) {
        // If there's no more room in the string buffer on the stack
        // for this parameter, allocate an array on the heap.
        if (string_array == &string_array_buf[0] &&
            string_array_index >= STRING_ARRAY_BUF_SIZE) {
          string_array = new nsAutoString[partCount - i];

          if (!string_array) {
            rv = NS_ERROR_OUT_OF_MEMORY;

            goto call_completion_end;
          }

          // Reset string_array_index now that we switched arrays.
          string_array_index = 0;
        }

        // Give the variant value a nsAString object to hold the data
        // in.
        vars->val.p =
          NS_STATIC_CAST(nsAString *, &string_array[string_array_index++]);
      }

      rv = WSPProxy::VariantToInParameter(listenerInterfaceInfo,
                                          mListenerMethodIndex, &paramInfo,
                                          value, vars);
      if (NS_FAILED(rv)) {
        goto call_completion_end;
      }
    }

    NS_ASSERTION(paramIndex == maxParamIndex,
                 "WSDL/IInfo param count mismatch");

    dispatchParams[paramIndex].val.p =
        NS_STATIC_CAST(nsIWebServiceCallContext*, this);
    dispatchParams[paramIndex].SetValIsInterface();
    dispatchParams[paramIndex].type.flags = 
      XPT_TDP_POINTER | TD_INTERFACE_TYPE;

    rv = XPTC_InvokeByIndex(mAsyncListener, mListenerMethodIndex,
                            paramCount, dispatchParams);
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

call_completion_end:
  if (headerCount) {
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(headerCount, headerBlocks);
  }
  if (bodyCount) {
    NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(bodyCount, bodyBlocks);
  }
  if(dispatchParams != paramBuffer) {
    delete [] dispatchParams;
  }
  if (string_array != &string_array_buf[0]) {
    delete [] string_array;
  }

  nsCOMPtr<nsIWebServiceCallContext> kungFuDeathGrip(this);
  mProxy->CallCompleted(this);
  NS_RELEASE(mProxy);

  return rv;
}
