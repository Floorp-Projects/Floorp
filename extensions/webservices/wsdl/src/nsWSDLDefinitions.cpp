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
#include "nsISOAPMessage.h"

////////////////////////////////////////////////////////////
//
// nsWSDLPort implementation
//
////////////////////////////////////////////////////////////
nsWSDLPort::nsWSDLPort(const nsAString& aName)
  : mName(aName)
{
}

nsWSDLPort::~nsWSDLPort()
{
}

NS_IMPL_ISUPPORTS1_CI(nsWSDLPort, nsIWSDLPort)

/* readonly attribute AString name; */
NS_IMETHODIMP 
nsWSDLPort::GetName(nsAString & aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute nsIDOMElement documentation; */
NS_IMETHODIMP 
nsWSDLPort::GetDocumentation(nsIDOMElement * *aDocumentation)
{
  NS_ENSURE_ARG_POINTER(aDocumentation);
  
  *aDocumentation = mDocumentationElement;
  NS_IF_ADDREF(*aDocumentation);
  
  return NS_OK;
}

/* readonly attribute PRUint32 operationCount; */
NS_IMETHODIMP 
nsWSDLPort::GetOperationCount(PRUint32 *aOperationCount)
{
  NS_ENSURE_ARG_POINTER(aOperationCount);

  return mOperations.Count(aOperationCount);
}

/* nsIWSDLOperation getOperation (in PRUint32 index); */
NS_IMETHODIMP 
nsWSDLPort::GetOperation(PRUint32 index, nsIWSDLOperation **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mOperations.QueryElementAt(index, NS_GET_IID(nsIWSDLOperation),
                                    (void**)_retval);
}

/* nsIWSDLOperation getOperationByName(in AString name); */
NS_IMETHODIMP
nsWSDLPort::GetOperationByName(const nsAString& aName,
                               nsIWSDLOperation** aOperation)
{
  nsresult rv;

  *aOperation = nsnull;

  // XXX Do a linear search for now. If more efficiency is needed
  // we can store the opeartions in a hash as well.
  PRUint32 index, count;
  mOperations.Count(&count);

  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIWSDLOperation> operation;
    
    rv = mOperations.QueryElementAt(index, NS_GET_IID(nsIWSDLOperation),
                                    getter_AddRefs(operation));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString name;
      operation->GetName(name);
      
      if (name.Equals(aName)) {
        *aOperation = operation;
        NS_ADDREF(*aOperation);
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLPort::GetBinding(nsIWSDLBinding** aBinding) 
{
  NS_ENSURE_ARG_POINTER(aBinding);

  *aBinding = mBinding;
  NS_IF_ADDREF(*aBinding);

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLPort::SetDocumentationElement(nsIDOMElement* aElement)
{
  mDocumentationElement = aElement;

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLPort::AddOperation(nsIWSDLOperation* aOperation)
{
  NS_ENSURE_ARG(aOperation);

  return mOperations.AppendElement(aOperation);
}

NS_IMETHODIMP
nsWSDLPort::SetBinding(nsIWSDLBinding* aBinding) 
{
  mBinding = aBinding;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSOAPPortBinding implementation
//
////////////////////////////////////////////////////////////
nsSOAPPortBinding::nsSOAPPortBinding(const nsAString& aName)
  : mName(aName), mStyle(STYLE_RPC), 
  mSoapVersion(nsISOAPPortBinding::SOAP_VERSION_UNKNOWN)
{
}

nsSOAPPortBinding::~nsSOAPPortBinding()
{
}

NS_IMPL_ISUPPORTS2_CI(nsSOAPPortBinding, 
                      nsIWSDLBinding, 
                      nsISOAPPortBinding)

/*  readonly attribute AString protocol; */
NS_IMETHODIMP
nsSOAPPortBinding::GetProtocol(nsAString& aProtocol)
{
  aProtocol.Assign(NS_LITERAL_STRING("soap"));
  
  return NS_OK;
}

/*  readonly attribute nsIDOMElement documentation; */
NS_IMETHODIMP
nsSOAPPortBinding::GetDocumentation(nsIDOMElement * *aDocumentation)
{
  NS_ENSURE_ARG_POINTER(aDocumentation);
  
  *aDocumentation = mDocumentationElement;
  NS_IF_ADDREF(*aDocumentation);
  
  return NS_OK;
}

/* readonly attribute AString bindingName; */
NS_IMETHODIMP 
nsSOAPPortBinding::GetName(nsAString & aBindingName)
{
  aBindingName.Assign(mName);

  return NS_OK;
}

/* readonly attribute AString address; */
NS_IMETHODIMP 
nsSOAPPortBinding::GetAddress(nsAString & aAddress)
{
  aAddress.Assign(mAddress);

  return NS_OK;
}

/* readonly attribute unsigned short style; */
NS_IMETHODIMP 
nsSOAPPortBinding::GetStyle(PRUint16 *aStyle)
{
  NS_ENSURE_ARG_POINTER(aStyle);

  *aStyle = mStyle;

  return NS_OK;
}

/* readonly attribute AString transport; */
NS_IMETHODIMP 
nsSOAPPortBinding::GetTransport(nsAString & aTransport)
{
  aTransport.Assign(mTransport);

  return NS_OK;
}

/* readonly attribute unsigned short soapVersion; */
NS_IMETHODIMP
nsSOAPPortBinding::GetSoapVersion(PRUint16 *aVersion)
{
  NS_ENSURE_ARG_POINTER(aVersion);
  *aVersion = mSoapVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsSOAPPortBinding::SetDocumentationElement(nsIDOMElement* aElement)
{
  mDocumentationElement = aElement;

  return NS_OK;
}

NS_IMETHODIMP
nsSOAPPortBinding::SetAddress(const nsAString& aAddress)
{
  mAddress.Assign(aAddress);

  return NS_OK;
}

NS_IMETHODIMP
nsSOAPPortBinding::SetStyle(PRUint16 aStyle) 
{
  mStyle = aStyle;

  return NS_OK;
}
  
NS_IMETHODIMP
nsSOAPPortBinding::SetTransport(const nsAString& aTransport)
{
  mTransport.Assign(aTransport);

  return NS_OK;
}

NS_IMETHODIMP
nsSOAPPortBinding::SetSoapVersion(PRUint16 aVersion)
{
  mSoapVersion = aVersion;
  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsWSDLOperation implementation
//
////////////////////////////////////////////////////////////
nsWSDLOperation::nsWSDLOperation(const nsAString &aName)
  : mName(aName)
{
}

nsWSDLOperation::~nsWSDLOperation()
{
}

NS_IMPL_ISUPPORTS1_CI(nsWSDLOperation, nsIWSDLOperation)

/* readonly attribute AString name; */
NS_IMETHODIMP 
nsWSDLOperation::GetName(nsAString & aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute nsIDOMElement documentation; */
NS_IMETHODIMP 
nsWSDLOperation::GetDocumentation(nsIDOMElement * *aDocumentation)
{
  NS_ENSURE_ARG_POINTER(aDocumentation);
  
  *aDocumentation = mDocumentationElement;
  NS_IF_ADDREF(*aDocumentation);
  
  return NS_OK;
}

/* readonly attribute nsIWSDLMessage input; */
NS_IMETHODIMP 
nsWSDLOperation::GetInput(nsIWSDLMessage * *aInput)
{
  NS_ENSURE_ARG_POINTER(aInput);

  *aInput = mInputMessage;
  NS_IF_ADDREF(*aInput);

  return NS_OK;
}

/* readonly attribute nsIWSDLMessage output; */
NS_IMETHODIMP 
nsWSDLOperation::GetOutput(nsIWSDLMessage * *aOutput)
{
  NS_ENSURE_ARG_POINTER(aOutput);

  *aOutput = mOutputMessage;
  NS_IF_ADDREF(*aOutput);

  return NS_OK;
}

/*  readonly attribute PRUint32 faultCount; */
NS_IMETHODIMP
nsWSDLOperation::GetFaultCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);

  return mFaultMessages.Count(aCount);
}

/*   nsIWSDLMessage getFault(in PRUint32 index); */
NS_IMETHODIMP 
nsWSDLOperation::GetFault(PRUint32 aIndex, nsIWSDLMessage * *aFault)
{
  NS_ENSURE_ARG_POINTER(aFault);

  return mFaultMessages.QueryElementAt(aIndex, NS_GET_IID(nsIWSDLMessage),
                                       (void**)aFault);
}

NS_IMETHODIMP
nsWSDLOperation::GetBinding(nsIWSDLBinding** aBinding) 
{
  NS_ENSURE_ARG_POINTER(aBinding);

  *aBinding = mBinding;
  NS_IF_ADDREF(*aBinding);

  return NS_OK;
}

/* readonly attribute PRUint32 parameterOrderCount; */
NS_IMETHODIMP 
nsWSDLOperation::GetParameterOrderCount(PRUint32 *aParameterCount)
{
  NS_ENSURE_ARG_POINTER(aParameterCount);

  *aParameterCount = (PRUint32)mParameters.Count();

  return NS_OK;
}

/* AString getParameter (in PRUint32 index); */
NS_IMETHODIMP 
nsWSDLOperation::GetParameter(PRUint32 index, nsAString & _retval)
{
  nsString* str = mParameters.StringAt((PRInt32)index);
  if (!str) {
    return NS_ERROR_FAILURE;
  }
  _retval.Assign(*str);

  return NS_OK;
}

/* PRUint32 getParameterIndex(in AString name); */
NS_IMETHODIMP
nsWSDLOperation::GetParameterIndex(const nsAString& aName,
                                   PRUint32* aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);

  *aIndex = (PRUint32)mParameters.IndexOf(aName);
  if (*aIndex == -1) {
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsWSDLOperation::SetDocumentationElement(nsIDOMElement* aElement)
{
  mDocumentationElement = aElement;

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLOperation::SetInput(nsIWSDLMessage* aInputMessage)
{
  mInputMessage =  aInputMessage;

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLOperation::SetOutput(nsIWSDLMessage* aOutputMessage)
{
  mOutputMessage = aOutputMessage;

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLOperation::AddFault(nsIWSDLMessage* aFaultMessage)
{
  NS_ENSURE_ARG(aFaultMessage);

  return mFaultMessages.AppendElement(aFaultMessage);
}

NS_IMETHODIMP
nsWSDLOperation::SetBinding(nsIWSDLBinding* aBinding) 
{
  mBinding = aBinding;

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLOperation::AddParameter(const nsAString& aParameter)
{
  mParameters.AppendString(aParameter);
  
  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSOAPOperationBinding implementation
//
////////////////////////////////////////////////////////////
nsSOAPOperationBinding::nsSOAPOperationBinding()
  : mStyle(nsISOAPPortBinding::STYLE_RPC)
{
}

nsSOAPOperationBinding::~nsSOAPOperationBinding()
{
}

NS_IMPL_ISUPPORTS2_CI(nsSOAPOperationBinding, 
                      nsIWSDLBinding, 
                      nsISOAPOperationBinding)

/*  readonly attribute AString protocol; */
NS_IMETHODIMP
nsSOAPOperationBinding::GetProtocol(nsAString& aProtocol)
{
  aProtocol.Assign(NS_LITERAL_STRING("soap"));
  
  return NS_OK;
}

/*  readonly attribute nsIDOMElement documentation; */
NS_IMETHODIMP
nsSOAPOperationBinding::GetDocumentation(nsIDOMElement * *aDocumentation)
{
  NS_ENSURE_ARG_POINTER(aDocumentation);
  
  *aDocumentation = mDocumentationElement;
  NS_IF_ADDREF(*aDocumentation);
  
  return NS_OK;
}

/* readonly attribute unsigned short style; */
NS_IMETHODIMP 
nsSOAPOperationBinding::GetStyle(PRUint16 *aStyle)
{
  NS_ENSURE_ARG_POINTER(aStyle);

  *aStyle = mStyle;

  return NS_OK;
}

/* readonly attribute AString soapAction; */
NS_IMETHODIMP 
nsSOAPOperationBinding::GetSoapAction(nsAString & aSoapAction)
{
  aSoapAction.Assign(mSoapAction);

  return NS_OK;
}

NS_IMETHODIMP
nsSOAPOperationBinding::SetDocumentationElement(nsIDOMElement* aElement)
{
  mDocumentationElement = aElement;

  return NS_OK;
}

NS_IMETHODIMP
nsSOAPOperationBinding::SetStyle(PRUint16 aStyle)
{
  mStyle = aStyle;

  return NS_OK;
}

NS_IMETHODIMP
nsSOAPOperationBinding::SetSoapAction(const nsAString& aAction)
{
  mSoapAction.Assign(aAction);
  
  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsWSDLMessage implementation
//
////////////////////////////////////////////////////////////
nsWSDLMessage::nsWSDLMessage(const nsAString& aName)
  : mName(aName)
{
}

nsWSDLMessage::~nsWSDLMessage()
{
}

NS_IMPL_ISUPPORTS1_CI(nsWSDLMessage, nsIWSDLMessage)

/* readonly attribute AString name; */
NS_IMETHODIMP 
nsWSDLMessage::GetName(nsAString & aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute nsIDOMElement documentation; */
NS_IMETHODIMP 
nsWSDLMessage::GetDocumentation(nsIDOMElement * *aDocumentation)
{
  NS_ENSURE_ARG_POINTER(aDocumentation);
  
  *aDocumentation = mDocumentationElement;
  NS_IF_ADDREF(*aDocumentation);
  
  return NS_OK;
}

NS_IMETHODIMP
nsWSDLMessage::GetBinding(nsIWSDLBinding** aBinding) 
{
  NS_ENSURE_ARG_POINTER(aBinding);

  *aBinding = mBinding;
  NS_IF_ADDREF(*aBinding);

  return NS_OK;
}

/* readonly attribute PRUint32 partCount; */
NS_IMETHODIMP 
nsWSDLMessage::GetPartCount(PRUint32 *aPartCount)
{
  NS_ENSURE_ARG_POINTER(aPartCount);

  return mParts.Count(aPartCount);
}

/* nsIWSDLPart getPart (in PRUint32 index); */
NS_IMETHODIMP 
nsWSDLMessage::GetPart(PRUint32 index, nsIWSDLPart **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  return mParts.QueryElementAt(index, NS_GET_IID(nsIWSDLPart),
                               (void**)_retval);

}

/* nsIWSDLPart getPartByName(in AString name); */
NS_IMETHODIMP
nsWSDLMessage::GetPartByName(const nsAString& aName,
                             nsIWSDLPart** aPart)
{
  nsresult rv;

  *aPart = nsnull;
  // XXX Do a linear search for now. If more efficiency is needed
  // we can store the part in a hash as well.
  PRUint32 index, count;
  mParts.Count(&count);

  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIWSDLPart> part;
    
    rv = mParts.QueryElementAt(index, NS_GET_IID(nsIWSDLPart),
                               getter_AddRefs(part));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString name;
      part->GetName(name);
      
      if (name.Equals(aName)) {
        *aPart = part;
        NS_ADDREF(*aPart);
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLMessage::SetDocumentationElement(nsIDOMElement* aElement)
{
  mDocumentationElement = aElement;

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLMessage::AddPart(nsIWSDLPart* aPart)
{
  NS_ENSURE_ARG(aPart);

  return mParts.AppendElement(aPart);
}

NS_IMETHODIMP
nsWSDLMessage::SetBinding(nsIWSDLBinding* aBinding) 
{
  mBinding = aBinding;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSOAPMessageBinding implementation
//
////////////////////////////////////////////////////////////
nsSOAPMessageBinding::nsSOAPMessageBinding(const nsAString& aNamespace)
  : mNamespace(aNamespace)
{
}

nsSOAPMessageBinding::~nsSOAPMessageBinding()
{
}

NS_IMPL_ISUPPORTS2_CI(nsSOAPMessageBinding, 
                      nsIWSDLBinding, 
                      nsISOAPMessageBinding)

/*  readonly attribute AString protocol; */
NS_IMETHODIMP
nsSOAPMessageBinding::GetProtocol(nsAString& aProtocol)
{
  aProtocol.Assign(NS_LITERAL_STRING("soap"));
  
  return NS_OK;
}

/*  readonly attribute nsIDOMElement documentation; */
NS_IMETHODIMP
nsSOAPMessageBinding::GetDocumentation(nsIDOMElement * *aDocumentation)
{
  NS_ENSURE_ARG_POINTER(aDocumentation);
  
  *aDocumentation = nsnull;
  
  return NS_OK;
}

/* readonly attribute AString namespace; */
NS_IMETHODIMP 
nsSOAPMessageBinding::GetNamespace(nsAString & aNamespace)
{
  aNamespace.Assign(mNamespace);

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsWSDLPart implementation
//
////////////////////////////////////////////////////////////
nsWSDLPart::nsWSDLPart(const nsAString& aName)
  : mName(aName)
{
}

nsWSDLPart::~nsWSDLPart()
{
}

NS_IMPL_ISUPPORTS1_CI(nsWSDLPart, nsIWSDLPart)

/* readonly attribute AString name; */
NS_IMETHODIMP 
nsWSDLPart::GetName(nsAString & aName)
{
  aName.Assign(mName);

  return NS_OK;
}

/* readonly attribute AString type; */
NS_IMETHODIMP 
nsWSDLPart::GetType(nsAString & aType)
{
  aType.Assign(mType);

  return NS_OK;
}

/* readonly attribute AString elementName; */
NS_IMETHODIMP 
nsWSDLPart::GetElementName(nsAString & aElementName)
{
  aElementName.Assign(mElementName);

  return NS_OK;
}

/* readonly attribute nsISchemaComponent schemaComponent; */
NS_IMETHODIMP 
nsWSDLPart::GetSchemaComponent(nsISchemaComponent * *aSchemaComponent)
{
  NS_ENSURE_ARG_POINTER(aSchemaComponent);

  *aSchemaComponent = mSchemaComponent;
  NS_IF_ADDREF(*aSchemaComponent);

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLPart::GetBinding(nsIWSDLBinding** aBinding) 
{
  NS_ENSURE_ARG_POINTER(aBinding);

  *aBinding = mBinding;
  NS_IF_ADDREF(*aBinding);

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLPart::SetTypeInfo(const nsAString& aType,
                        const nsAString& aElementName,
                        nsISchemaComponent* aSchemaComponent)
{
  mType.Assign(aType);
  mElementName.Assign(aElementName);
  mSchemaComponent = aSchemaComponent;

  return NS_OK;
}

NS_IMETHODIMP
nsWSDLPart::SetBinding(nsIWSDLBinding* aBinding) 
{
  mBinding = aBinding;

  return NS_OK;
}

////////////////////////////////////////////////////////////
//
// nsSOAPPartBinding implementation
//
////////////////////////////////////////////////////////////
nsSOAPPartBinding::nsSOAPPartBinding(PRUint16 aLocation, PRUint16 aUse,
                                     const nsAString& aEncodingStyle,
                                     const nsAString& aNamespace)
  : mLocation(aLocation), mUse(aUse), 
  mEncodingStyle(aEncodingStyle), mNamespace(aNamespace)
{
}

nsSOAPPartBinding::~nsSOAPPartBinding()
{
}

NS_IMPL_ISUPPORTS2_CI(nsSOAPPartBinding, 
                      nsIWSDLBinding, 
                      nsISOAPPartBinding)

/*  readonly attribute AString protocol; */
NS_IMETHODIMP
nsSOAPPartBinding::GetProtocol(nsAString& aProtocol)
{
  aProtocol.Assign(NS_LITERAL_STRING("soap"));
  
  return NS_OK;
}

/*  readonly attribute nsIDOMElement documentation; */
NS_IMETHODIMP
nsSOAPPartBinding::GetDocumentation(nsIDOMElement * *aDocumentation)
{
  NS_ENSURE_ARG_POINTER(aDocumentation);
  
  *aDocumentation = nsnull;
  
  return NS_OK;
}

/* readonly attribute unsigned short location; */
NS_IMETHODIMP 
nsSOAPPartBinding::GetLocation(PRUint16 *aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);

  *aLocation = mLocation;

  return NS_OK;
}

/* readonly attribute unsigned short use; */
NS_IMETHODIMP 
nsSOAPPartBinding::GetUse(PRUint16 *aUse)
{
  NS_ENSURE_ARG_POINTER(aUse);

  *aUse = mUse;

  return NS_OK;
}

/* readonly attribute AString encodingStyle; */
NS_IMETHODIMP 
nsSOAPPartBinding::GetEncodingStyle(nsAString & aEncodingStyle)
{
  aEncodingStyle.Assign(mEncodingStyle);

  return NS_OK;
}

/* readonly attribute AString namespace; */
NS_IMETHODIMP 
nsSOAPPartBinding::GetNamespace(nsAString & aNamespace)
{
  aNamespace.Assign(mNamespace);

  return NS_OK;
}
