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

#include "nsSOAPFault.h"
#include "nsSOAPUtils.h"
#include "nsIDOMNodeList.h"
#include "nsISOAPMessage.h"
#include "nsSOAPException.h"

nsSOAPFault::nsSOAPFault()
{
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
    if (name.Equals(gSOAPStrings->kFaultTagName)) {
      if (namespaceURI.
          Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_2])) {
        mVersion = nsISOAPMessage::VERSION_1_2;
      } else if (namespaceURI.
                 Equals(*gSOAPStrings->kSOAPEnvURI[nsISOAPMessage::VERSION_1_1])) {
        mVersion = nsISOAPMessage::VERSION_1_1;
      } else {
        return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_BADFAULT", "Cannot recognize SOAP version from namespace URI of fault");
      }
    } else {
      return SOAP_EXCEPTION(NS_ERROR_ILLEGAL_VALUE,"SOAP_BADFAULT", "Cannot recognize element tag of fault.");
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
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;
  aFaultCode.Truncate();
  nsCOMPtr<nsIDOMElement> faultcode;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
                                       gSOAPStrings->kEmpty,
                                       gSOAPStrings->kFaultCodeTagName,
                                       getter_AddRefs(faultcode));
  if (faultcode) {
    nsAutoString combined;
    nsresult rc = nsSOAPUtils::GetElementTextContent(faultcode, combined);
    if (NS_FAILED(rc))
      return rc;
    return nsSOAPUtils::GetLocalName(combined, aFaultCode);
  }
  return NS_OK;
}

/* readonly attribute wstring faultNamespaceURI; */
NS_IMETHODIMP nsSOAPFault::GetFaultNamespaceURI(nsAString & aNamespaceURI)
{
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;
  aNamespaceURI.Truncate();
  nsCOMPtr<nsIDOMElement> faultcode;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
                                       gSOAPStrings->kEmpty,
                                       gSOAPStrings->kFaultCodeTagName,
                                       getter_AddRefs(faultcode));
  if (faultcode) {
    nsAutoString combined;
    nsresult rc = nsSOAPUtils::GetElementTextContent(faultcode, combined);
    if (NS_FAILED(rc))
      return rc;
    return nsSOAPUtils::GetNamespaceURI(nsnull, faultcode, combined, aNamespaceURI);
  }
  return NS_OK;
}

/* readonly attribute wstring faultString; */
NS_IMETHODIMP nsSOAPFault::GetFaultString(nsAString & aFaultString)
{
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;

  aFaultString.Truncate();
  nsCOMPtr<nsIDOMElement> element;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
                                       gSOAPStrings->kEmpty,
                                       gSOAPStrings->kFaultStringTagName,
                                       getter_AddRefs(element));
  if (element) {
    nsresult rc = nsSOAPUtils::GetElementTextContent(element, aFaultString);
    if (NS_FAILED(rc))
      return rc;
  }
  return NS_OK;
}

/* readonly attribute wstring faultActor; */
NS_IMETHODIMP nsSOAPFault::GetFaultActor(nsAString & aFaultActor)
{
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;

  aFaultActor.Truncate();
  nsCOMPtr<nsIDOMElement> element;
  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
                                       gSOAPStrings->kEmpty,
                                       gSOAPStrings->kFaultActorTagName,
                                       getter_AddRefs(element));
  if (element) {
    nsresult rc = nsSOAPUtils::GetElementTextContent(element, aFaultActor);
    if (NS_FAILED(rc))
      return rc;
  }
  return NS_OK;
}

/* readonly attribute nsIDOMElement detail; */
NS_IMETHODIMP nsSOAPFault::GetDetail(nsIDOMElement * *aDetail)
{
  NS_ENSURE_ARG_POINTER(aDetail);
  if (!mFaultElement)
    return NS_ERROR_ILLEGAL_VALUE;

  nsSOAPUtils::GetSpecificChildElement(nsnull, mFaultElement,
                                       gSOAPStrings->kEmpty,
                                       gSOAPStrings->kFaultDetailTagName,
                                       aDetail);
  return NS_OK;
}
