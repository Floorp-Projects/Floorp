/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsSOAPFault.h"
#include "nsSOAPUtils.h"
#include "nsIDOMNodeList.h"
#include "nsISOAPMessage.h"

static NS_NAMED_LITERAL_STRING(kEmpty, "");

nsSOAPFault::nsSOAPFault()
{
  NS_INIT_ISUPPORTS();
}

nsSOAPFault::~nsSOAPFault()
{
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPFault, nsISOAPFault)
/* attribute nsIDOMElement element; */
NS_IMETHODIMP nsSOAPFault::SetElement(nsIDOMElement * aElement)
{
  if (aElement) {
    nsAutoString namespaceURI;
    nsAutoString name;
    nsresult rc = aElement->GetNamespaceURI(namespaceURI);
    if (NS_FAILED(rc))
      return rc;
    rc = aElement->GetLocalName(name);
    if (NS_FAILED(rc))
      return rc;
    if (name.Equals(nsSOAPUtils::kFaultTagName)) {
      if (namespaceURI.
	  Equals(*nsSOAPUtils::kSOAPEnvURI[nsISOAPMessage::VERSION_1_2])) {
	mVersion = nsISOAPMessage::VERSION_1_2;
      } else if (namespaceURI.
		 Equals(*nsSOAPUtils::
			kSOAPEnvURI[nsISOAPMessage::VERSION_1_1])) {
	mVersion = nsISOAPMessage::VERSION_1_1;
      } else {
	return NS_ERROR_ILLEGAL_VALUE;
      }
    } else {
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }
  mFaultElement = aElement;
  return NS_OK;
}

NS_IMETHODIMP nsSOAPFault::GetElement(nsIDOMElement * *aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = mFaultElement;
  NS_IF_ADDREF(*aElement);
  return NS_OK;
}

/* readonly attribute wstring faultCode; */
NS_IMETHODIMP nsSOAPFault::GetFaultCode(nsAString & aFaultCode)
{
  NS_ENSURE_ARG_POINTER(&aFaultCode);
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;
  aFaultCode.Truncate();
  nsCOMPtr < nsIDOMElement > faultcode;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
				       kEmpty,
				       nsSOAPUtils::kFaultCodeTagName,
				       getter_AddRefs(faultcode));
  if (faultcode) {
    nsAutoString combined;
    nsSOAPUtils::GetElementTextContent(faultcode, combined);
    return nsSOAPUtils::GetLocalName(combined, aFaultCode);
  }
  return NS_OK;
}

/* readonly attribute wstring faultNamespaceURI; */
NS_IMETHODIMP nsSOAPFault::GetFaultNamespaceURI(nsAString & aNamespaceURI)
{
  NS_ENSURE_ARG_POINTER(&aNamespaceURI);
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;
  aNamespaceURI.Truncate();
  nsCOMPtr < nsIDOMElement > faultcode;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
				       kEmpty,
				       nsSOAPUtils::kFaultCodeTagName,
				       getter_AddRefs(faultcode));
  if (faultcode) {
    nsAutoString combined;
    nsSOAPUtils::GetElementTextContent(faultcode, combined);
    return nsSOAPUtils::GetNamespaceURI(nsnull, faultcode, combined, aNamespaceURI);
  }
  return NS_OK;
}

/* readonly attribute wstring faultString; */
NS_IMETHODIMP nsSOAPFault::GetFaultString(nsAString & aFaultString)
{
  NS_ENSURE_ARG_POINTER(&aFaultString);
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;

  aFaultString.Truncate();
  nsCOMPtr < nsIDOMElement > element;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
				       kEmpty,
				       nsSOAPUtils::kFaultStringTagName,
				       getter_AddRefs(element));
  if (element) {
    nsSOAPUtils::GetElementTextContent(element, aFaultString);
  }
  return NS_OK;
}

/* readonly attribute wstring faultActor; */
NS_IMETHODIMP nsSOAPFault::GetFaultActor(nsAString & aFaultActor)
{
  NS_ENSURE_ARG_POINTER(&aFaultActor);
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;

  aFaultActor.Truncate();
  nsCOMPtr < nsIDOMElement > element;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
				       kEmpty,
				       nsSOAPUtils::kFaultActorTagName,
				       getter_AddRefs(element));
  if (element) {
    nsSOAPUtils::GetElementTextContent(element, aFaultActor);
  }
  return NS_OK;
}

/* readonly attribute nsIDOMElement detail; */
NS_IMETHODIMP nsSOAPFault::GetDetail(nsIDOMElement * *aDetail)
{
  NS_ENSURE_ARG_POINTER(aDetail);
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;

  nsCOMPtr < nsIDOMElement > element;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
				       kEmpty,
				       nsSOAPUtils::kFaultDetailTagName,
				       aDetail);
  return NS_OK;
}
