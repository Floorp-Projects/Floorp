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

nsSOAPFault::nsSOAPFault(nsIDOMElement* aElement)
{
  NS_INIT_ISUPPORTS();
  mFaultElement = aElement;
}

nsSOAPFault::~nsSOAPFault()
{
}

NS_IMPL_ISUPPORTS2_CI(nsSOAPFault, nsISOAPFault, nsISecurityCheckedComponent)

/* attribute nsIDOMElement element; */
NS_IMETHODIMP nsSOAPFault::SetElement(nsIDOMElement *aElement)
{
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
  aFaultCode.Truncate();
  nsCOMPtr<nsIDOMElement> faultcode;
  nsSOAPUtils::GetSpecificChildElement(mFaultElement, 
                                       nsSOAPUtils::kSOAPEnvURI, 
                                       nsSOAPUtils::kFaultCodeTagName, 
                                       getter_AddRefs(faultcode));
  if (faultcode) {
    nsSOAPUtils::GetElementTextContent(faultcode, aFaultCode);
  }
  return NS_OK;
}

/* readonly attribute wstring faultString; */
NS_IMETHODIMP nsSOAPFault::GetFaultString(nsAString & aFaultString)
{
  NS_ENSURE_ARG_POINTER(&aFaultString);

  aFaultString.Truncate();
  nsCOMPtr<nsIDOMElement> element;
  nsSOAPUtils::GetSpecificChildElement(mFaultElement, nsSOAPUtils::kSOAPEnvURI,
    nsSOAPUtils::kFaultStringTagName, getter_AddRefs(element));
  if (element) {
    nsSOAPUtils::GetElementTextContent(element, aFaultString);
  }
  return NS_OK;
}

/* readonly attribute wstring faultActor; */
NS_IMETHODIMP nsSOAPFault::GetFaultActor(nsAString & aFaultActor)
{
  NS_ENSURE_ARG_POINTER(&aFaultActor);

  aFaultActor.Truncate();
  nsCOMPtr<nsIDOMElement> element;
  nsSOAPUtils::GetSpecificChildElement(mFaultElement, nsSOAPUtils::kSOAPEnvURI,
    nsSOAPUtils::kFaultActorTagName, getter_AddRefs(element));
  if (element) {
    nsSOAPUtils::GetElementTextContent(element, aFaultActor);
  }
  return NS_OK;
}

/* readonly attribute nsIDOMElement detail; */
NS_IMETHODIMP nsSOAPFault::GetDetail(nsIDOMElement * *aDetail)
{
  NS_ENSURE_ARG_POINTER(aDetail);

  nsCOMPtr<nsIDOMElement> element;
  nsSOAPUtils::GetSpecificChildElement(mFaultElement, nsSOAPUtils::kSOAPEnvURI,
    nsSOAPUtils::kFaultDetailTagName, aDetail);
  return NS_OK;
}

static const char* kAllAccess = "AllAccess";

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP 
nsSOAPFault::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPFault))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP 
nsSOAPFault::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPFault))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPFault::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPFault))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP 
nsSOAPFault::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  if (iid->Equals(NS_GET_IID(nsISOAPFault))) {
    *_retval = nsCRT::strdup(kAllAccess);
  }

  return NS_OK;
}
