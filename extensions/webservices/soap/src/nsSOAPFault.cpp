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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

NS_IMPL_ISUPPORTS2(nsSOAPFault, nsISOAPFault, nsISecurityCheckedComponent)

/* readonly attribute nsIDOMElement element; */
NS_IMETHODIMP nsSOAPFault::GetElement(nsIDOMElement * *aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = mFaultElement;
  NS_IF_ADDREF(*aElement);
  return NS_OK;
}

/* readonly attribute wstring faultCode; */
NS_IMETHODIMP nsSOAPFault::GetFaultCode(PRUnichar * *aFaultCode)
{
  NS_ENSURE_ARG_POINTER(aFaultCode);
  
  *aFaultCode = nsnull;
  if (mFaultElement) {
    nsCOMPtr<nsIDOMNodeList> list;

    nsAutoString tagname;
    tagname.AssignWithConversion(nsSOAPUtils::kFaultCodeTagName);
    mFaultElement->GetElementsByTagName(tagname, getter_AddRefs(list));
    PRUint32 length;
    list->GetLength(&length);
    if (length > 0) {
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsIDOMElement> element;
      
      list->Item(0, getter_AddRefs(node));
      element = do_QueryInterface(node);

      nsAutoString text;
      nsSOAPUtils::GetElementTextContent(element, text);
      if (text.Length() > 0) {
	*aFaultCode = text.ToNewUnicode();
	if (!*aFaultCode) return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return NS_OK;
}

/* readonly attribute wstring faultString; */
NS_IMETHODIMP nsSOAPFault::GetFaultString(PRUnichar * *aFaultString)
{
  NS_ENSURE_ARG_POINTER(aFaultString);
  
  *aFaultString = nsnull;
  if (mFaultElement) {
    nsCOMPtr<nsIDOMNodeList> list;

    nsAutoString tagname;
    tagname.AssignWithConversion(nsSOAPUtils::kFaultStringTagName);
    mFaultElement->GetElementsByTagName(tagname, getter_AddRefs(list));
    PRUint32 length;
    list->GetLength(&length);
    if (length > 0) {
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsIDOMElement> element;
      
      list->Item(0, getter_AddRefs(node));
      element = do_QueryInterface(node);

      nsAutoString text;
      nsSOAPUtils::GetElementTextContent(element, text);
      if (text.Length() > 0) {
	*aFaultString = text.ToNewUnicode();
	if (!*aFaultString) return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return NS_OK;
}

/* readonly attribute wstring faultActor; */
NS_IMETHODIMP nsSOAPFault::GetFaultActor(PRUnichar * *aFaultActor)
{
  NS_ENSURE_ARG_POINTER(aFaultActor);
  
  *aFaultActor = nsnull;
  if (mFaultElement) {
    nsCOMPtr<nsIDOMNodeList> list;

    nsAutoString tagname;
    tagname.AssignWithConversion(nsSOAPUtils::kFaultActorTagName);
    mFaultElement->GetElementsByTagName(tagname, getter_AddRefs(list));
    PRUint32 length;
    list->GetLength(&length);
    if (length > 0) {
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsIDOMElement> element;
      
      list->Item(0, getter_AddRefs(node));
      element = do_QueryInterface(node);

      nsAutoString text;
      nsSOAPUtils::GetElementTextContent(element, text);
      if (text.Length() > 0) {
	*aFaultActor = text.ToNewUnicode();
	if (!*aFaultActor) return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return NS_OK;
}

/* readonly attribute nsIDOMElement detail; */
NS_IMETHODIMP nsSOAPFault::GetDetail(nsIDOMElement * *aDetail)
{
  NS_ENSURE_ARG_POINTER(aDetail);

  *aDetail = nsnull;
  if (mFaultElement) {
    nsCOMPtr<nsIDOMNodeList> list;

    nsAutoString tagname;
    tagname.AssignWithConversion(nsSOAPUtils::kFaultDetailTagName);
    mFaultElement->GetElementsByTagName(tagname, getter_AddRefs(list));
    PRUint32 length;
    list->GetLength(&length);
    if (length > 0) {
      nsCOMPtr<nsIDOMNode> node;
      nsCOMPtr<nsIDOMElement> element;
      
      list->Item(0, getter_AddRefs(node));
      return node->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aDetail);
    }
  }
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
