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

#include "nsSOAPUtils.h"
#include "nsIDOMText.h"
#include "nsCOMPtr.h"
#include "nsIJSContextStack.h"
#include "nsISOAPParameter.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsISupportsArray.h"

const char* nsSOAPUtils::kSOAPEnvURI = "http://schemas.xmlsoap.org/soap/envelope/";
const char* nsSOAPUtils::kSOAPEncodingURI = "http://schemas.xmlsoap.org/soap/encoding/";
const char* nsSOAPUtils::kSOAPEnvPrefix = "SOAP-ENV";
const char* nsSOAPUtils::kSOAPEncodingPrefix = "SOAP-ENC";
const char* nsSOAPUtils::kXSIURI = "http://www.w3.org/1999/XMLSchema-instance";
const char* nsSOAPUtils::kXSDURI = "http://www.w3.org/1999/XMLSchema";
const char* nsSOAPUtils::kXSIPrefix = "xsi";
const char* nsSOAPUtils::kXSDPrefix = "xsd";
const char* nsSOAPUtils::kEncodingStyleAttribute = "encodingStyle";
const char* nsSOAPUtils::kEnvelopeTagName = "Envelope";
const char* nsSOAPUtils::kHeaderTagName = "Header";
const char* nsSOAPUtils::kBodyTagName = "Body";
const char* nsSOAPUtils::kFaultTagName = "Fault";
const char* nsSOAPUtils::kFaultCodeTagName = "faultcode";
const char* nsSOAPUtils::kFaultStringTagName = "faultstring";
const char* nsSOAPUtils::kFaultActorTagName = "faultactor";
const char* nsSOAPUtils::kFaultDetailTagName = "detail";

void
nsSOAPUtils::GetFirstChildElement(nsIDOMElement* aParent, 
                                  nsIDOMElement** aElement)
{
  nsCOMPtr<nsIDOMNode> child;

  *aElement = nsnull;
  aParent->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 type;
    child->GetNodeType(&type);
    if (nsIDOMNode::ELEMENT_NODE == type) {
      child->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aElement);
      break;
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
  }
}

void
nsSOAPUtils::GetNextSiblingElement(nsIDOMElement* aStart, 
                                   nsIDOMElement** aElement)
{
  nsCOMPtr<nsIDOMNode> sibling;

  *aElement = nsnull;
  aStart->GetNextSibling(getter_AddRefs(sibling));
  while (sibling) {
    PRUint16 type;
    sibling->GetNodeType(&type);
    if (nsIDOMNode::ELEMENT_NODE == type) {
      sibling->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aElement);
      break;
    }
    nsCOMPtr<nsIDOMNode> temp = sibling;
    temp->GetNextSibling(getter_AddRefs(sibling));
  }
}

void 
nsSOAPUtils::GetElementTextContent(nsIDOMElement* aElement, 
                                   nsString& aText)
{
  nsCOMPtr<nsIDOMNode> child;
  
  aText.Truncate();
  aElement->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 type;
    child->GetNodeType(&type);
    if (nsIDOMNode::TEXT_NODE == type) {
      nsCOMPtr<nsIDOMText> text = do_QueryInterface(child);
      nsAutoString data;
      text->GetData(data);
      aText.Append(data);
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
  }
}

PRBool
nsSOAPUtils::HasChildElements(nsIDOMElement* aElement)
{
  nsCOMPtr<nsIDOMNode> child;

  aElement->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 type;
    child->GetNodeType(&type);
    if (nsIDOMNode::ELEMENT_NODE == type) {
      return PR_TRUE;
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
  }

  return PR_FALSE;
}

void
nsSOAPUtils::GetInheritedEncodingStyle(nsIDOMElement* aEntry, 
                                       char** aEncodingStyle)
{
  nsCOMPtr<nsIDOMNode> node = aEntry;

  while (node) {
    nsAutoString value;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
    if (element) {
      element->GetAttributeNS(NS_ConvertASCIItoUCS2(nsSOAPUtils::kSOAPEnvURI), 
                              NS_ConvertASCIItoUCS2(nsSOAPUtils::kEncodingStyleAttribute),
                              value);
      if (value.Length() > 0) {
        *aEncodingStyle = value.ToNewCString();
        return;
      }
    }
    nsCOMPtr<nsIDOMNode> temp = node;
    temp->GetParentNode(getter_AddRefs(node));
  }
  *aEncodingStyle = nsCRT::strdup(kSOAPEncodingURI);
}

JSContext*
nsSOAPUtils::GetSafeContext()
{
  // Get the "safe" JSContext: our JSContext of last resort
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack = 
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
    return nsnull;
  nsCOMPtr<nsIThreadJSContextStack> tcs = do_QueryInterface(stack);
  JSContext* cx;
  if (NS_FAILED(tcs->GetSafeJSContext(&cx))) {
    return nsnull;
  }
  return cx;
}
 
JSContext*
nsSOAPUtils::GetCurrentContext()
{
  // Get JSContext from stack.
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack = 
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
    return nsnull;
  JSContext *cx;
  if (NS_FAILED(stack->Peek(&cx)))
    return nsnull;
  return cx;
}

nsresult 
nsSOAPUtils::ConvertValueToJSVal(JSContext* aContext, 
                                 nsISupports* aValue, 
                                 JSObject* aJSValue, 
                                 PRInt32 aType,
                                 jsval* vp)
{
  *vp = JSVAL_NULL;
  switch(aType) {
    case nsISOAPParameter::PARAMETER_TYPE_VOID:
      *vp = JSVAL_VOID;
      break;

    case nsISOAPParameter::PARAMETER_TYPE_STRING:
    {
      nsCOMPtr<nsISupportsWString> wstr = do_QueryInterface(aValue);
      if (!wstr) return NS_ERROR_FAILURE;
      
      nsXPIDLString data;
      wstr->GetData(getter_Copies(data));
      
      if (data) {
        JSString* jsstr = JS_NewUCStringCopyZ(aContext,
                                             NS_REINTERPRET_CAST(const jschar*, (const PRUnichar*)data));
        if (jsstr) {
          *vp = STRING_TO_JSVAL(jsstr);
        }
      }
      break;
    }

    case nsISOAPParameter::PARAMETER_TYPE_BOOLEAN:
    {
      nsCOMPtr<nsISupportsPRBool> prb = do_QueryInterface(aValue);
      if (!prb) return NS_ERROR_FAILURE;

      PRBool data;
      prb->GetData(&data);

      if (data) {
        *vp = JSVAL_TRUE;
      }
      else {
        *vp = JSVAL_FALSE;
      }
      break;
    }

    case nsISOAPParameter::PARAMETER_TYPE_DOUBLE:
    {
      nsCOMPtr<nsISupportsDouble> dub = do_QueryInterface(aValue);
      if (!dub) return NS_ERROR_FAILURE;

      double data;
      dub->GetData(&data);

      double* dataPtr = JS_NewDouble(aContext, (jsdouble)data); 
      *vp = DOUBLE_TO_JSVAL(dataPtr);

      break;
    }

    case nsISOAPParameter::PARAMETER_TYPE_FLOAT:
    {
      nsCOMPtr<nsISupportsFloat> flt = do_QueryInterface(aValue);
      if (!flt) return NS_ERROR_FAILURE;

      float data;
      flt->GetData(&data);

      double* dataPtr = JS_NewDouble(aContext, (jsdouble)data); 
      *vp = DOUBLE_TO_JSVAL(dataPtr);
      
      break;
    }

    case nsISOAPParameter::PARAMETER_TYPE_LONG:
    {
      // XXX How to express 64-bit values in JavaScript?
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    case nsISOAPParameter::PARAMETER_TYPE_INT:
    {
      nsCOMPtr<nsISupportsPRInt32> isupint32 = do_QueryInterface(aValue);
      if (!isupint32) return NS_ERROR_FAILURE;

      PRInt32 data;
      isupint32->GetData(&data);
      
      *vp = INT_TO_JSVAL(data);
      
      break;
    }

    case nsISOAPParameter::PARAMETER_TYPE_SHORT:
    {
      nsCOMPtr<nsISupportsPRInt16> isupint16 = do_QueryInterface(aValue);
      if (!isupint16) return NS_ERROR_FAILURE;

      PRInt16 data;
      isupint16->GetData(&data);
      
      *vp = INT_TO_JSVAL((PRInt32)data);
      
      break;
    }
    
    case nsISOAPParameter::PARAMETER_TYPE_BYTE:
    {
      nsCOMPtr<nsISupportsChar> isupchar = do_QueryInterface(aValue);
      if (!isupchar) return NS_ERROR_FAILURE;

      char data;
      isupchar->GetData(&data);
      
      *vp = INT_TO_JSVAL((PRInt32)data);

      break;
    }

    case nsISOAPParameter::PARAMETER_TYPE_ARRAY:
    {
      nsCOMPtr<nsISupportsArray> array = do_QueryInterface(aValue);
      if (!array) return NS_ERROR_FAILURE;

      JSObject* arrayobj = JS_NewArrayObject(aContext, 0, nsnull);
      if (!arrayobj) return NS_ERROR_FAILURE;

      PRUint32 index, count;
      array->Count(&count);

      for (index = 0; index < count; index++) {
        nsCOMPtr<nsISupports> isup = getter_AddRefs(array->ElementAt(index));
        nsCOMPtr<nsISOAPParameter> param = do_QueryInterface(isup);
        if (!param) return NS_ERROR_FAILURE;

        nsCOMPtr<nsISupports> paramVal;
        JSObject* paramObj;
        PRInt32 paramType;

        param->GetValueAndType(getter_AddRefs(paramVal), &paramType);
        param->GetJSValue(&paramObj);

        jsval val;
        nsresult rv = ConvertValueToJSVal(aContext, paramVal, paramObj,
                                          paramType, &val);
        if (NS_FAILED(rv)) return rv;

        JS_SetElement(aContext, arrayobj, (jsint)index, &val);
      }

      *vp = OBJECT_TO_JSVAL(arrayobj);
      break;
    }

    case nsISOAPParameter::PARAMETER_TYPE_JAVASCRIPT_ARRAY:
    case nsISOAPParameter::PARAMETER_TYPE_JAVASCRIPT_OBJECT:
    {
      *vp = OBJECT_TO_JSVAL(aJSValue);
      break;
    }
  }

  return NS_OK;
}

nsresult 
nsSOAPUtils::ConvertJSValToValue(JSContext* aContext,
                                 jsval val, 
                                 nsISupports** aValue,
                                 JSObject** aJSValue,
                                 PRInt32* aType)
{
  *aValue = nsnull;
  *aJSValue = nsnull;
  if (JSVAL_IS_NULL(val)) {
    *aType = nsISOAPParameter::PARAMETER_TYPE_NULL;
  }
  else if (JSVAL_IS_VOID(val)) {
    *aType = nsISOAPParameter::PARAMETER_TYPE_VOID;
  }
  else if (JSVAL_IS_STRING(val)) {
    JSString* jsstr;
    *aType = nsISOAPParameter::PARAMETER_TYPE_STRING;
    
    jsstr = JSVAL_TO_STRING(val);
    if (jsstr) {
      nsCOMPtr<nsISupportsWString> wstr = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
      if (!wstr) return NS_ERROR_FAILURE;

      PRUnichar* data = NS_REINTERPRET_CAST(PRUnichar*, 
                                            JS_GetStringChars(jsstr));
      if (data) {
        wstr->SetData(data);
      }
      *aValue = wstr;
      NS_ADDREF(*aValue);
    }
  }
  else if (JSVAL_IS_DOUBLE(val)) {
    *aType = nsISOAPParameter::PARAMETER_TYPE_DOUBLE;
    
    nsCOMPtr<nsISupportsDouble> dub = do_CreateInstance(NS_SUPPORTS_DOUBLE_CONTRACTID);
    if (!dub) return NS_ERROR_FAILURE;

    dub->SetData((double)(*JSVAL_TO_DOUBLE(val)));
    *aValue = dub;
    NS_ADDREF(*aValue);
  }
  else if (JSVAL_IS_INT(val)) {
    *aType = nsISOAPParameter::PARAMETER_TYPE_INT;
    
    nsCOMPtr<nsISupportsPRInt32> isupint = do_CreateInstance(NS_SUPPORTS_PRINT32_CONTRACTID);
    if (!isupint) return NS_ERROR_FAILURE;
    
    isupint->SetData((PRInt32)JSVAL_TO_INT(val));
    *aValue = isupint;
    NS_ADDREF(*aValue);
  }
  else if (JSVAL_IS_BOOLEAN(val)) {
    *aType = nsISOAPParameter::PARAMETER_TYPE_BOOLEAN;
    
    nsCOMPtr<nsISupportsPRBool> isupbool = do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);
    if (!isupbool) return NS_ERROR_FAILURE;

    isupbool->SetData((PRBool)JSVAL_TO_BOOLEAN(val));
    *aValue = isupbool;
    NS_ADDREF(*aValue);
  }
  else if (JSVAL_IS_OBJECT(val)) {
    JSObject* jsobj = JSVAL_TO_OBJECT(val);
    if (JS_IsArrayObject(aContext, jsobj)) {
      *aType = nsISOAPParameter::PARAMETER_TYPE_JAVASCRIPT_ARRAY;
    }
    else {
      *aType = nsISOAPParameter::PARAMETER_TYPE_JAVASCRIPT_OBJECT;
    }
    *aJSValue = jsobj;
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}
